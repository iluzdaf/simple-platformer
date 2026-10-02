#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/actor/actor_system.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_system.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"
#include "simple_platformer/world/world_simulation.hpp"
#include "support/tile_map_builder.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/add_player.hpp"
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
    const float bodySide = GENERATE(12.0F, static_cast<float>(tests::TileSize));
    simple_platformer::Actor npc = tests::ActorBuilder::sized({bodySide, bodySide})
                                       .atFeet(first)
                                       .platforming()
                                       .climbing({60.0F})
                                       .patrolling(first, second)
                                       .thinking({});
    tests::platformerMovement(npc).grounded = true;
    const simple_platformer::ActorId npcId = world.addActor(npc);

    bool climbedCeiling = false;
    bool reachedSecondFloor = false;
    bool returnedToFirstFloor = false;
    for (int tick = 0; tick < 4000 && !returnedToFirstFloor; ++tick)
    {
        simple_platformer::updateWorldSimulation(map, world, tests::FixedStepSeconds);
        climbedCeiling = climbedCeiling || tests::surfaceClimb(world, npcId).surface ==
                                               simple_platformer::ClimbSurface::Ceiling;
        const auto cell = simple_platformer::cellAtFeet(
            tests::TileSize, simple_platformer::feetOf(tests::actor(world, npcId).body.bounds));
        reachedSecondFloor = reachedSecondFloor || cell == simple_platformer::Cell{11, 5};
        returnedToFirstFloor = reachedSecondFloor && cell == simple_platformer::Cell{2, 5};
    }
    CAPTURE(bodySide, reachedSecondFloor, returnedToFirstFloor);
    REQUIRE(climbedCeiling);
    REQUIRE(reachedSecondFloor);
    REQUIRE(returnedToFirstFloor);
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
                                       .platforming()
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
        return tests::ActorBuilder::sized({12.0F, 12.0F}).atFeet(feet).platforming();
    }
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
                                          .platforming()
                                          .thinking({64.0F, 1.0F}));

    tests::platformerMovement(world, npcId).grounded = true;
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastKnownTargetFeet = {40.0F, 32.0F};
    tests::perception(world, npcId).targetVisible = false;
    simple_platformer::updateNpcBehaviour(map, world, tests::FixedStepSeconds);
    const auto& path = pathFollower(world, npcId).path;
    if (!path.has_value())
    {
        FAIL("The NPC must have a planned path");
        return;
    }

    const auto* plannedWaypoints = path->waypoints.data();
    REQUIRE_FALSE(path->waypoints.empty());

    // With the path planned and the map as it was, the next step keeps it.
    simple_platformer::updateNpcBehaviour(map, world, tests::FixedStepSeconds);

    if (!path.has_value())
    {
        FAIL("The NPC must have a planned path");
        return;
    }
    REQUIRE(path->waypoints.data() == plannedWaypoints);

    // A break may have cut the path, so it is planned again though the goal is the same.
    REQUIRE(map.breakTile({7, 2}));
    simple_platformer::updateNpcBehaviour(map, world, tests::FixedStepSeconds);
    REQUIRE(pathFollower(world, npcId).breaksWhenPlanned == 1);
}

TEST_CASE("An unreachable patrol stays still without a partial path", "[npc][navigation]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"....#....", "....#....", "#########"});
    simple_platformer::World world;
    tests::addPlayer(world, makePlayer({22.0F, 12.0F}));
    const auto npcId =
        world.addActor(makeNpc({24.0F, 32.0F}).patrolling({24.0F, 32.0F}, {120.0F, 32.0F}));
    for (int tick = 0; tick < 2; ++tick)
    {
        simple_platformer::updateNpcBehaviour(map, world, 0.1F);
        const auto& follower = tests::pathFollower(world, npcId);
        REQUIRE_FALSE(follower.path.has_value());
        REQUIRE(follower.goal == simple_platformer::feetInCell(tests::TileSize, {7, 1}));
        REQUIRE(tests::actor(world, npcId).intentions.direction == glm::vec2{});
    }
}

TEST_CASE("An unreachable replacement goal clears the old path", "[npc][navigation]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"....#....", "....#....", "#########"});
    simple_platformer::World world;
    const auto npcId =
        world.addActor(makeNpc({24.0F, 32.0F}).patrolling({24.0F, 32.0F}, {56.0F, 32.0F}));
    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(tests::pathFollower(world, npcId).path.has_value());
    tests::patrol(world, npcId).secondFeet = {120.0F, 32.0F};
    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE_FALSE(tests::pathFollower(world, npcId).path.has_value());
    REQUIRE(tests::actor(world, npcId).intentions.direction == glm::vec2{});
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
