#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <vector>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/actor/actor_system.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/navigation/platformer_connection_cache.hpp"
#include "simple_platformer/navigation/navigation_fill.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/route.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_system.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/timing/frame_profile.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"
#include "simple_platformer/world/world_simulation.hpp"
#include "support/tile_map_builder.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/add_player.hpp"
#include "support/prepare_navigation_cache.hpp"
#include "support/fixed_step.hpp"
#include "support/tile_size.hpp"

namespace
{
    tests::ActorBuilder::Thinking makeNpc(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F})
            .atFeet(feet)
            .flying(20.0F)
            .thinking({64.0F, 1.0F});
    }
}

TEST_CASE("A climbing NPC patrols over a wall and ceiling", "[npc][navigation][climb]")
{
    simple_platformer::TileMap map = tests::TileMapBuilder({"..............",
                                                            "..cccccccccc..",
                                                            ".c..........c.",
                                                            ".c.########.c.",
                                                            ".c.########.c.",
                                                            ".c.########.c.",
                                                            "..##########..",
                                                            ".............."})
                                         .where('c', tests::Tile{}.blocksMovement().climbable());
    simple_platformer::World world;
    const glm::vec2 first = simple_platformer::feetInCell(tests::TileSize, {2, 5});
    const glm::vec2 second = simple_platformer::feetInCell(tests::TileSize, {11, 5});
    simple_platformer::Actor npc = tests::ActorBuilder::sized({12.0F, 12.0F})
                                       .atFeet(first)
                                       .walking()
                                       .climbing({60.0F})
                                       .patrolling(first, second)
                                       .thinking({});
    tests::platformerMovement(npc).grounded = true;
    const simple_platformer::ActorId npcId = world.addActor(npc);

    bool climbedCeiling = false;
    bool reachedSecondFloor = false;
    for (int tick = 0; tick < 2000 && !reachedSecondFloor; ++tick)
    {
        simple_platformer::updateWorldSimulation(map, world, tests::FixedStepSeconds);
        climbedCeiling = climbedCeiling || tests::surfaceClimb(world, npcId).surface ==
                                               simple_platformer::ClimbSurface::Ceiling;
        reachedSecondFloor = !tests::patrol(world, npcId).headingToSecond;
    }
    REQUIRE(climbedCeiling);
    REQUIRE(reachedSecondFloor);
}

TEST_CASE("A climbing NPC holds the ceiling at the end of its patrol", "[npc][navigation][climb]")
{
    simple_platformer::TileMap map =
        tests::TileMapBuilder(
            {"cccccccccc", "c........c", "c........c", "c........c", "c........c", "cccccccccc"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    simple_platformer::World world;
    const glm::vec2 onFloor = simple_platformer::feetInCell(tests::TileSize, {5, 4});
    const glm::vec2 underCeiling = simple_platformer::feetInCell(tests::TileSize, {6, 1});
    simple_platformer::Actor npc = tests::ActorBuilder::sized({8.0F, 8.0F})
                                       .atFeet(onFloor)
                                       .walking()
                                       .climbing({60.0F})
                                       .patrolling(onFloor, underCeiling)
                                       .thinking({});
    tests::platformerMovement(npc).grounded = true;
    const simple_platformer::ActorId npcId = world.addActor(npc);

    bool reachedCeilingEnd = false;
    for (int tick = 0; tick < 2000 && !reachedCeilingEnd; ++tick)
    {
        simple_platformer::updateWorldSimulation(map, world, tests::FixedStepSeconds);
        reachedCeilingEnd = !tests::patrol(world, npcId).headingToSecond;
    }
    REQUIRE(reachedCeilingEnd);
    REQUIRE(tests::surfaceClimb(world, npcId).surface == simple_platformer::ClimbSurface::Ceiling);

    // Turning back for the floor starts along the ceiling, not by letting go.
    for (int tick = 0; tick < 10; ++tick)
    {
        simple_platformer::updateWorldSimulation(map, world, tests::FixedStepSeconds);
        REQUIRE(
            tests::surfaceClimb(world, npcId).surface == simple_platformer::ClimbSurface::Ceiling);
    }
}

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

TEST_CASE("A walking NPC's search reads the fill's cache and never simulates", "[npc][navigation]")
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
    const simple_platformer::PlatformerConnectionCache& cache = world.platformerConnections();

    // Before the fill, the search stores nothing: it queues the cell it needs and waits.
    const simple_platformer::FrameProfile waiting = profiledNpcUpdate(map, world);
    REQUIRE(simple_platformer::frameStatisticCount(waiting, "Path searches") == 1);
    REQUIRE(simple_platformer::frameStatisticCount(waiting, "Paths deferred") == 1);
    REQUIRE(cache.size() == 0);
    REQUIRE_FALSE(pathFollower(world, npcId).path.has_value());

    // Once the fill has cached the map, the search finds a route and writes nothing.
    tests::prepareNavigationCache(map, world);
    const std::size_t cachedAfterFill = cache.size();
    const simple_platformer::FrameProfile searched = profiledNpcUpdate(map, world);
    REQUIRE(simple_platformer::frameStatisticCount(searched, "Path searches") == 1);
    REQUIRE(simple_platformer::frameStatisticCount(searched, "Paths deferred") == 0);
    REQUIRE(simple_platformer::frameStatisticCount(searched, "Cells expanded") > 0);
    REQUIRE(pathFollower(world, npcId).path.has_value());
    REQUIRE(cache.size() == cachedAfterFill);
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
    // it met one and gave up rather than simulate it, and the NPC asks again next step.
    REQUIRE(simple_platformer::frameStatisticCount(waiting, "Tile breaks applied") == 1);
    REQUIRE(
        simple_platformer::frameStatisticCount(waiting, "Cells dropped") ==
        static_cast<int>(world.platformerConnections().cellsPending(profile)));
    REQUIRE(simple_platformer::frameStatisticCount(waiting, "Path searches") == 1);
    REQUIRE(simple_platformer::frameStatisticCount(waiting, "Paths deferred") == 1);
    REQUIRE(world.platformerConnections().cellsPending(profile) > 0);
    REQUIRE_FALSE(pathFollower(world, npcId).path.has_value());

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
    const std::vector<simple_platformer::RouteConnection>* overTheHole =
        world.platformerConnections().cachedConnections({2, 1}, profile);
    REQUIRE(overTheHole != nullptr);
    REQUIRE(overTheHole->empty());
    const simple_platformer::FrameProfile searched = profiledNpcUpdate(map, world);
    REQUIRE(simple_platformer::frameStatisticCount(searched, "Path searches") == 1);
    REQUIRE(simple_platformer::frameStatisticCount(searched, "Paths deferred") == 0);
    REQUIRE(pathFollower(world, npcId).path.has_value());
}

TEST_CASE("An NPC plans its path again after a break", "[npc][navigation]")
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

    // With the path planned and the map as it was, the next step searches nothing.
    const simple_platformer::FrameProfile settled = profiledNpcUpdate(map, world);
    REQUIRE(simple_platformer::frameStatisticCount(settled, "Path searches") == 0);

    // A break may have cut the path, so it is planned again though the goal is the same.
    REQUIRE(map.breakTile({7, 2}));
    const simple_platformer::FrameProfile broken = profiledNpcUpdate(map, world);
    REQUIRE(simple_platformer::frameStatisticCount(broken, "Path searches") == 1);
    REQUIRE(pathFollower(world, npcId).breaksWhenPlanned == 1);
}

TEST_CASE("An unreachable patrol heads as close as it can without retrying", "[npc][navigation]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"....#....", "....#....", "#########"});
    simple_platformer::World world;
    tests::addPlayer(world, makePlayer({22.0F, 12.0F}));
    const simple_platformer::ActorId npcId =
        world.addActor(makeNpc({24.0F, 32.0F}).patrolling({24.0F, 32.0F}, {120.0F, 32.0F}));

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    const simple_platformer::PathFollower& follower = tests::pathFollower(world, npcId);
    REQUIRE(follower.path.has_value());
    const glm::vec2 closest = simple_platformer::feetInCell(tests::TileSize, {3, 1});
    REQUIRE(
        simple_platformer::endOf(follower.path.value_or(simple_platformer::NavigationPath{})) ==
        closest);
    REQUIRE(follower.goal == simple_platformer::feetInCell(tests::TileSize, {7, 1}));
    REQUIRE(tests::actor(world, npcId).intentions.direction.x > 0.0F);

    // The path still serves the same goal, so the NPC keeps following it.
    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(follower.path.has_value());
    REQUIRE(follower.nextStep == 0);
    REQUIRE(
        simple_platformer::endOf(follower.path.value_or(simple_platformer::NavigationPath{})) ==
        closest);
}

TEST_CASE("A patrol goal that moves is planned for at once", "[npc][navigation]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({".........", ".........", "#########"});
    simple_platformer::World world;
    tests::addPlayer(world, makePlayer({22.0F, 12.0F}));
    const simple_platformer::ActorId npcId =
        world.addActor(makeNpc({24.0F, 32.0F}).patrolling({24.0F, 32.0F}, {120.0F, 32.0F}));

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    const simple_platformer::PathFollower& follower = tests::pathFollower(world, npcId);
    REQUIRE(follower.goal == simple_platformer::feetInCell(tests::TileSize, {7, 1}));

    // A few pixels is not worth planning again; half a tile is.
    tests::patrol(world, npcId).secondFeet = {124.0F, 32.0F};
    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(follower.goal == simple_platformer::feetInCell(tests::TileSize, {7, 1}));

    tests::patrol(world, npcId).secondFeet = {88.0F, 32.0F};
    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(follower.goal == simple_platformer::feetInCell(tests::TileSize, {5, 1}));
}
