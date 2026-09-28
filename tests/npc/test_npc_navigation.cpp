#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <vector>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/actor/actor_system.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/connection_cache.hpp"
#include "simple_platformer/navigation/navigation_fill.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_system.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/timing/frame_profile.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"
#include "support/tile_map_builder.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/prepare_navigation_cache.hpp"
#include "support/fixed_step.hpp"

using tests::brain;
using tests::pathFollower;

namespace
{
    tests::ActorBuilder makePlayer(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F}).atFeet(feet).walking();
    }

    simple_platformer::FrameProfile profiledNpcUpdate(
        const simple_platformer::TileMap& map,
        simple_platformer::World& world)
    {
        simple_platformer::FrameProfile profile;
        simple_platformer::updateNpcBehaviour(
            map, world, tests::FixedStepSeconds, nullptr, &profile);
        return profile;
    }

}

TEST_CASE("A walking NPC's searches fill the world's connection cache", "[npc][navigation]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::World world;
    const auto playerId = world.addActor(makePlayer({70.0F, 32.0F}));
    const auto npcId = world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                                          .atFeet({56.0F, 32.0F})
                                          .walking()
                                          .thinking({64.0F, 1.0F}));
    tests::platformerMovement(world, npcId).grounded = true;
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastKnownTargetFeet = {8.0F, 32.0F};
    tests::perception(world, npcId).targetVisible = false;
    REQUIRE(world.platformerConnections().size() == 0);

    const simple_platformer::FrameProfile first = profiledNpcUpdate(map, world);
    REQUIRE(simple_platformer::frameStatisticCount(first, "Path searches") == 1);
    REQUIRE(simple_platformer::frameStatisticCount(first, "Cells reused") == 0);
    REQUIRE(simple_platformer::frameStatisticCount(first, "Search simulated ticks") > 0);
    REQUIRE(world.platformerConnections().size() > 0);

    // A search to a new destination reuses what the first one simulated.
    brain(world, npcId).lastKnownTargetFeet = {24.0F, 32.0F};
    pathFollower(world, npcId).repathRemaining = 0.0F;
    const simple_platformer::FrameProfile second = profiledNpcUpdate(map, world);
    REQUIRE(simple_platformer::frameStatisticCount(second, "Path searches") == 1);
    REQUIRE(simple_platformer::frameStatisticCount(second, "Cells reused") > 0);
    REQUIRE(simple_platformer::frameStatisticCount(second, "Search simulated ticks") == 0);

    // Back to the first destination, the path is remembered and nothing is expanded.
    brain(world, npcId).lastKnownTargetFeet = {8.0F, 32.0F};
    pathFollower(world, npcId).repathRemaining = 0.0F;
    const simple_platformer::FrameProfile third = profiledNpcUpdate(map, world);
    REQUIRE(simple_platformer::frameStatisticCount(third, "Path searches") == 1);
    REQUIRE(simple_platformer::frameStatisticCount(third, "Paths remembered") == 1);
    REQUIRE(simple_platformer::frameStatisticCount(third, "Cells expanded") == 0);
}

TEST_CASE("An NPC's search after a break waits for the fill and asks again", "[npc][navigation]")
{
    simple_platformer::TileMap map =
        tests::TileMapBuilder({".....", ".....", "##g##"})
            .where('g', tests::Tile().blocksMovement().breaksInto('.'));
    simple_platformer::World world;
    const auto playerId = world.addActor(makePlayer({70.0F, 32.0F}));
    const auto npcId = world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                                          .atFeet({8.0F, 32.0F})
                                          .walking()
                                          .thinking({64.0F, 1.0F}));
    tests::prepareNavigationCache(map, world);
    const simple_platformer::PlatformerTraversalProfile profile{
        {12.0F, 12.0F}, simple_platformer::PlatformerMovementConfig{}, tests::FixedStepSeconds};
    REQUIRE(world.platformerConnections().cachedConnections({2, 1}, profile) != nullptr);

    REQUIRE(map.breakTile({2, 2}));
    tests::platformerMovement(world, npcId).grounded = true;
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastKnownTargetFeet = {72.0F, 32.0F};
    tests::perception(world, npcId).targetVisible = false;
    const simple_platformer::FrameProfile waiting = profiledNpcUpdate(map, world);

    // The search synced with the map first, so the cells the break touched were dropped;
    // it met one and gave up rather than simulate it, and the NPC asks again next step
    // instead of waiting out its cooldown.
    REQUIRE(simple_platformer::frameStatisticCount(waiting, "Path searches") == 1);
    REQUIRE(simple_platformer::frameStatisticCount(waiting, "Paths deferred") == 1);
    REQUIRE(simple_platformer::frameStatisticCount(waiting, "Search simulated ticks") == 0);
    REQUIRE(world.platformerConnections().cellsPending(profile) > 0);
    REQUIRE_FALSE(pathFollower(world, npcId).path.has_value());
    REQUIRE(pathFollower(world, npcId).repathRemaining == 0.0F);

    // The fill recaches the dropped cells over the steps that follow, charged to
    // the profile, and the next search goes through. The cell over the hole, which
    // nothing can stand on now, is cached as having no connections.
    int filledTicks = 0;
    const std::size_t pending = world.platformerConnections().cellsPending(profile);
    for (std::size_t step = 0;
         step < pending && world.platformerConnections().cellsPending(profile) > 0;
         ++step)
    {
        simple_platformer::FrameProfile fillProfile;
        simple_platformer::advanceNavigationFill(
            map,
            world.platformerConnections(),
            simple_platformer::NavigationFillTicksPerStep,
            &fillProfile);
        filledTicks += simple_platformer::frameStatisticCount(fillProfile, "Fill simulated ticks");
    }
    REQUIRE(filledTicks > 0);
    REQUIRE(world.platformerConnections().cellsPending(profile) == 0);
    const std::vector<simple_platformer::NavigationConnection>* overTheHole =
        world.platformerConnections().cachedConnections({2, 1}, profile);
    REQUIRE(overTheHole != nullptr);
    REQUIRE(overTheHole->empty());
    const simple_platformer::FrameProfile searched = profiledNpcUpdate(map, world);
    REQUIRE(simple_platformer::frameStatisticCount(searched, "Path searches") == 1);
    REQUIRE(simple_platformer::frameStatisticCount(searched, "Paths deferred") == 0);
    REQUIRE(pathFollower(world, npcId).repathRemaining > 0.0F);
}

TEST_CASE("An NPC plans its path again after a break, cooldown or not", "[npc][navigation]")
{
    simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "..........", "#######g##"})
            .where('g', tests::Tile().blocksMovement().breaksInto('.'));
    simple_platformer::World world;
    const auto playerId = world.addActor(makePlayer({40.0F, 32.0F}));
    const auto npcId = world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                                          .atFeet({8.0F, 32.0F})
                                          .walking()
                                          .thinking({64.0F, 1.0F}));
    tests::prepareNavigationCache(map, world);
    tests::platformerMovement(world, npcId).grounded = true;
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastKnownTargetFeet = {40.0F, 32.0F};
    tests::perception(world, npcId).targetVisible = false;
    const simple_platformer::FrameProfile planned = profiledNpcUpdate(map, world);
    REQUIRE(simple_platformer::frameStatisticCount(planned, "Path searches") == 1);
    REQUIRE(pathFollower(world, npcId).path.has_value());
    REQUIRE(pathFollower(world, npcId).repathRemaining > 0.0F);

    // With the path planned and the map as it was, the next step searches nothing.
    const simple_platformer::FrameProfile settled = profiledNpcUpdate(map, world);
    REQUIRE(simple_platformer::frameStatisticCount(settled, "Path searches") == 0);

    // A break may have cut the path, so it is planned again before the cooldown is up.
    REQUIRE(map.breakTile({7, 2}));
    const simple_platformer::FrameProfile broken = profiledNpcUpdate(map, world);
    REQUIRE(simple_platformer::frameStatisticCount(broken, "Path searches") == 1);
    REQUIRE(pathFollower(world, npcId).breaksWhenPlanned == 1);
}
