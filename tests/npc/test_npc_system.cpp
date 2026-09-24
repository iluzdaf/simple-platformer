#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/actor/actor_system.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/connection_cache.hpp"
#include "simple_platformer/navigation/navigation_fill.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_system.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"
#include "support/require_near.hpp"
#include "support/tile_map_builder.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/add_player.hpp"
#include "support/fill_navigation.hpp"
#include "support/fixed_step.hpp"

using tests::actor;
using tests::bite;
using tests::brain;
using tests::pathFollower;
using tests::patrol;

namespace
{
    // An actor the world can treat as the player.
    tests::ActorBuilder makePlayer(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F}).atFeet(feet).walking();
    }

    // The NPC these tests measure their maps against.
    tests::ActorBuilder makeNpc(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F})
            .atFeet(feet)
            .flying(20.0F)
            .thinking({64.0F, 1.0F});
    }
}

TEST_CASE("NPC behaviour rejects invalid timing", "[npc][validation]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"...", "...", "###"});
    simple_platformer::World world;

    REQUIRE_THROWS_AS(
        simple_platformer::updateNpcBehaviour(map, world, -0.1F), std::invalid_argument);
}

TEST_CASE("A chasing NPC patrols again once it has no target", "[npc][fsm]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"............", "............", "............", "############"});
    simple_platformer::World world;
    const simple_platformer::ActorId npcId =
        world.addActor(makeNpc({22.0F, 28.0F}).patrolling({24.0F, 32.0F}, {72.0F, 32.0F}));
    brain(world, npcId).state = simple_platformer::NpcState::Chase;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Patrol);
}

TEST_CASE("A chasing NPC follows the last seen target feet", "[npc][fsm]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "########"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = tests::addPlayer(world, makePlayer({70.0F, 28.0F}));
    const simple_platformer::ActorId npcId = world.addActor(makeNpc({24.0F, 32.0F}));
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastSeenTargetFeet = {72.0F, 32.0F};
    brain(world, npcId).targetVisible = false;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);

    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Chase);
    REQUIRE(actor(world, npcId).intentions.direction.x > 0.0F);
    REQUIRE_FALSE(actor(world, npcId).intentions.primaryAttackPressed);
}

TEST_CASE(
    "Ground pursuit resolves remembered feet without tracking the hidden player",
    "[npc][fsm]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::World world;
    // The player is now to the right, but the last sighting was to the left.
    const auto playerId = world.addActor(makePlayer({70.0F, 32.0F}));
    // A walking NPC as tall as a zombie, standing on the floor at y = 32. Its height decides
    // where it can stand.
    const auto npcId = world.addActor(tests::ActorBuilder::sized({12.0F, 20.0F})
                                          .atFeet({56.0F, 32.0F})
                                          .walking()
                                          .thinking({64.0F, 1.0F}));
    tests::platformerMovement(world, npcId).grounded = true;
    const glm::vec2 lastSeenFeet{8.0F, 20.0F};
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastSeenTargetFeet = lastSeenFeet;
    brain(world, npcId).targetVisible = false;

    simple_platformer::updateNpcBehaviour(map, world, tests::FixedStepSeconds);

    REQUIRE(actor(world, npcId).intentions.direction.x < 0.0F);
    REQUIRE(pathFollower(world, npcId).destinationCell == simple_platformer::GridPosition{0, 1});
    REQUIRE(brain(world, npcId).lastSeenTargetFeet == lastSeenFeet);
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
    brain(world, npcId).lastSeenTargetFeet = {8.0F, 32.0F};
    brain(world, npcId).targetVisible = false;
    REQUIRE(world.platformerConnections().size() == 0);

    const simple_platformer::NpcBehaviourCost first =
        simple_platformer::updateNpcBehaviour(map, world, tests::FixedStepSeconds);
    REQUIRE(first.pathSearches == 1);
    REQUIRE(first.searches.cellsReused == 0);
    REQUIRE(first.searches.simulatedTicks > 0);
    REQUIRE(world.platformerConnections().size() > 0);

    // A search to a new destination reuses what the first one simulated.
    brain(world, npcId).lastSeenTargetFeet = {24.0F, 32.0F};
    pathFollower(world, npcId).repathRemaining = 0.0F;
    const simple_platformer::NpcBehaviourCost second =
        simple_platformer::updateNpcBehaviour(map, world, tests::FixedStepSeconds);
    REQUIRE(second.pathSearches == 1);
    REQUIRE(second.searches.cellsReused > 0);
    REQUIRE(second.searches.simulatedTicks == 0);

    // Back to the first destination, the path is remembered and nothing is expanded.
    brain(world, npcId).lastSeenTargetFeet = {8.0F, 32.0F};
    pathFollower(world, npcId).repathRemaining = 0.0F;
    const simple_platformer::NpcBehaviourCost third =
        simple_platformer::updateNpcBehaviour(map, world, tests::FixedStepSeconds);
    REQUIRE(third.pathSearches == 1);
    REQUIRE(third.searches.pathsRemembered == 1);
    REQUIRE(third.searches.nodesExpanded == 0);
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
    tests::fillNavigation(map, world);
    const simple_platformer::ConnectionBody body{
        {12.0F, 12.0F}, simple_platformer::PlatformerMovementConfig{}, tests::FixedStepSeconds};
    REQUIRE(world.platformerConnections().find({2, 1}, body) != nullptr);

    REQUIRE(map.breakTile({2, 2}));
    tests::platformerMovement(world, npcId).grounded = true;
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastSeenTargetFeet = {72.0F, 32.0F};
    brain(world, npcId).targetVisible = false;
    const simple_platformer::NpcBehaviourCost waiting =
        simple_platformer::updateNpcBehaviour(map, world, tests::FixedStepSeconds);

    // The search synced with the map first, so the cells the break touched were dropped;
    // it met one and gave up rather than simulate it, and the NPC asks again next step
    // instead of waiting out its cooldown.
    REQUIRE(waiting.pathSearches == 1);
    REQUIRE(waiting.searches.deferred == 1);
    REQUIRE(waiting.searches.simulatedTicks == 0);
    REQUIRE(world.platformerConnections().cellsPending(body) > 0);
    REQUIRE_FALSE(pathFollower(world, npcId).path.has_value());
    REQUIRE(pathFollower(world, npcId).repathRemaining == 0.0F);

    // The fill keeps the dropped cells again over the steps that follow, charged to
    // the profile, and the next search goes through. The cell over the hole, which
    // nothing can stand on now, is kept as having no connections.
    int filledTicks = 0;
    const std::size_t pending = world.platformerConnections().cellsPending(body);
    for (std::size_t step = 0;
         step < pending && world.platformerConnections().cellsPending(body) > 0;
         ++step)
    {
        filledTicks +=
            simple_platformer::fillNavigation(
                map, world.platformerConnections(), simple_platformer::NavigationFillTicksPerStep)
                .simulatedTicks;
    }
    REQUIRE(filledTicks > 0);
    REQUIRE(world.platformerConnections().cellsPending(body) == 0);
    const std::vector<simple_platformer::NavigationNeighbor>* overTheHole =
        world.platformerConnections().find({2, 1}, body);
    REQUIRE(overTheHole != nullptr);
    REQUIRE(overTheHole->empty());
    const simple_platformer::NpcBehaviourCost searched =
        simple_platformer::updateNpcBehaviour(map, world, tests::FixedStepSeconds);
    REQUIRE(searched.pathSearches == 1);
    REQUIRE(searched.searches.deferred == 0);
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
    tests::fillNavigation(map, world);
    tests::platformerMovement(world, npcId).grounded = true;
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastSeenTargetFeet = {40.0F, 32.0F};
    brain(world, npcId).targetVisible = false;
    const simple_platformer::NpcBehaviourCost planned =
        simple_platformer::updateNpcBehaviour(map, world, tests::FixedStepSeconds);
    REQUIRE(planned.pathSearches == 1);
    REQUIRE(pathFollower(world, npcId).path.has_value());
    REQUIRE(pathFollower(world, npcId).repathRemaining > 0.0F);

    // With the path planned and the map as it was, the next step searches nothing.
    const simple_platformer::NpcBehaviourCost settled =
        simple_platformer::updateNpcBehaviour(map, world, tests::FixedStepSeconds);
    REQUIRE(settled.pathSearches == 0);

    // A break may have cut the path, so it is planned again before the cooldown is up.
    REQUIRE(map.breakTile({7, 2}));
    const simple_platformer::NpcBehaviourCost broken =
        simple_platformer::updateNpcBehaviour(map, world, tests::FixedStepSeconds);
    REQUIRE(broken.pathSearches == 1);
    REQUIRE(pathFollower(world, npcId).breaksWhenPlanned == 1);
}

TEST_CASE("An NPC enters bite once and returns to chase after recovery", "[npc][fsm]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = tests::addPlayer(world, makePlayer({38.0F, 28.0F}));
    const simple_platformer::ActorId npcId =
        world.addActor(makeNpc({22.0F, 28.0F}).onTeam(simple_platformer::Team::Enemy).biting());
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastSeenTargetFeet = {38.0F, 28.0F};
    brain(world, npcId).targetVisible = true;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Bite);
    REQUIRE(actor(world, npcId).intentions.primaryAttackPressed);

    simple_platformer::WorldRequests requests;
    simple_platformer::updateAttacks(world, requests, 0.1F);
    REQUIRE(bite(world, npcId).phase == simple_platformer::BitePhase::Windup);

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE_FALSE(actor(world, npcId).intentions.primaryAttackPressed);
    simple_platformer::updateAttacks(world, requests, 1.0F);
    REQUIRE(bite(world, npcId).phase == simple_platformer::BitePhase::Ready);

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Chase);
}

TEST_CASE("An NPC without a bite continues chasing at close range", "[npc][fsm]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = tests::addPlayer(world, makePlayer({38.0F, 28.0F}));
    const simple_platformer::ActorId npcId = world.addActor(makeNpc({22.0F, 28.0F}));
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastSeenTargetFeet = {38.0F, 28.0F};
    brain(world, npcId).targetVisible = true;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);

    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Chase);
    REQUIRE_FALSE(actor(world, npcId).intentions.primaryAttackPressed);
    REQUIRE(actor(world, npcId).intentions.direction.x > 0.0F);
}

TEST_CASE("A ranged NPC stops and requests an attack while its target is visible", "[npc][fsm]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = tests::addPlayer(world, makePlayer({54.0F, 12.0F}));
    const simple_platformer::ActorId npcId =
        world.addActor(makeNpc({22.0F, 28.0F}).onTeam(simple_platformer::Team::Enemy).shooting());
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastSeenTargetFeet = {54.0F, 12.0F};
    brain(world, npcId).targetVisible = true;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    // Facing is decided by the movement update from what the NPC intends.
    simple_platformer::updateActorMovement(map, world, 0.1F);

    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Chase);
    REQUIRE(actor(world, npcId).facing == simple_platformer::Facing::Right);
    REQUIRE(actor(world, npcId).intentions.direction == glm::vec2{0.0F, 0.0F});
    REQUIRE(actor(world, npcId).intentions.aimDirection == glm::vec2{32.0F, -16.0F});
    REQUIRE(actor(world, npcId).intentions.primaryAttackPressed);
}

TEST_CASE("A patrol path produces intentions that move the flying NPC", "[npc][fsm]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "........", "########"});
    simple_platformer::World world;
    tests::addPlayer(world, makePlayer({102.0F, 44.0F}));
    const simple_platformer::ActorId npcId =
        world.addActor(makeNpc({24.0F, 32.0F}).patrolling({24.0F, 32.0F}, {72.0F, 32.0F}));

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Patrol);
    REQUIRE(actor(world, npcId).intentions.direction.x > 0.0F);

    const float previousX = actor(world, npcId).body.bounds.position.x;
    simple_platformer::updateActorMovement(map, world, 0.1F);
    REQUIRE(actor(world, npcId).body.bounds.position.x > previousX);
}

TEST_CASE("A patrol swaps endpoints after reaching its destination", "[npc][fsm]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::World world;
    tests::addPlayer(world, makePlayer({70.0F, 12.0F}));
    const simple_platformer::ActorId npcId =
        world.addActor(makeNpc({24.0F, 32.0F}).patrolling({24.0F, 32.0F}, {56.0F, 32.0F}));
    patrol(world, npcId).headingToSecond = false;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);

    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Patrol);
    REQUIRE(patrol(world, npcId).headingToSecond);
    REQUIRE_FALSE(pathFollower(world, npcId).path.has_value());
}

TEST_CASE("An unreachable patrol waits before retrying its path", "[npc][fsm]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"....#....", "....#....", "#########"});
    simple_platformer::World world;
    tests::addPlayer(world, makePlayer({22.0F, 12.0F}));
    const simple_platformer::ActorId npcId =
        world.addActor(makeNpc({24.0F, 32.0F}).patrolling({24.0F, 32.0F}, {120.0F, 32.0F}));

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE_FALSE(pathFollower(world, npcId).path.has_value());
    REQUIRE(pathFollower(world, npcId).destinationCell == simple_platformer::GridPosition{7, 1});
    REQUIRE_NEAR(pathFollower(world, npcId).repathRemaining, 0.25F);

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE_NEAR(pathFollower(world, npcId).repathRemaining, 0.15F);
    simple_platformer::updateNpcBehaviour(map, world, 0.2F);
    REQUIRE_NEAR(pathFollower(world, npcId).repathRemaining, 0.25F);
}
