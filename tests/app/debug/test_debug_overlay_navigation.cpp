#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "debug/debug_overlay.hpp"
#include "debug/navigation_debug.hpp"
#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/actor_builder.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"
#include "support/fixed_step.hpp"
#include "support/neighbor_with.hpp"

TEST_CASE("Debug overlay data describes path connections and progress", "[app][debug]")
{
    simple_platformer::PathFollower follower;
    follower.path = simple_platformer::NavigationPath{
        {1, 2},
        {{{3, 2}, simple_platformer::Traversal::Walk, {}},
         {{4, 1}, simple_platformer::Traversal::Jump, {}},
         {{4, 3}, simple_platformer::Traversal::Fall, {}}}};
    follower.nextStep = 1;
    follower.destinationCell = simple_platformer::GridPosition{4, 3};

    simple_platformer::Actor npc =
        tests::ActorBuilder::sized({12.0F, 12.0F}).at({16.0F, 32.0F}).walking().thinking({});
    npc.pathFollower = follower;

    simple_platformer::World world;
    world.addActor(npc);
    const simple_platformer::TileMap map = tests::TileMapBuilder({"......", "######"});
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};

    const simple_platformer::DebugOverlay debug = simple_platformer::makeDebugOverlay(
        world, map, cameraController, 128.0F, tests::FixedStepSeconds);

    REQUIRE(debug.actors.size() == 1);
    REQUIRE(debug.actors.front().pathFollower.has_value());
    const simple_platformer::PathFollowerDebugInfo path =
        debug.actors.front().pathFollower.value_or(simple_platformer::PathFollowerDebugInfo{});
    REQUIRE(path.hasPath);
    REQUIRE(path.nextStep == 1);
    REQUIRE(path.stepCount == 3);
    REQUIRE(path.destinationFeet == simple_platformer::feetInCell(tests::TileSize, {4, 3}));
    REQUIRE(path.connections.size() == 3);

    REQUIRE(path.connections[0].fromFeet == simple_platformer::feetInCell(tests::TileSize, {1, 2}));
    REQUIRE(path.connections[0].toFeet == simple_platformer::feetInCell(tests::TileSize, {3, 2}));
    REQUIRE(path.connections[0].traversal == simple_platformer::Traversal::Walk);
    REQUIRE(path.connections[0].completed);
    REQUIRE_FALSE(path.connections[0].next);

    REQUIRE(path.connections[1].fromFeet == simple_platformer::feetInCell(tests::TileSize, {3, 2}));
    REQUIRE(path.connections[1].toFeet == simple_platformer::feetInCell(tests::TileSize, {4, 1}));
    REQUIRE(path.connections[1].traversal == simple_platformer::Traversal::Jump);
    REQUIRE_FALSE(path.connections[1].completed);
    REQUIRE(path.connections[1].next);

    REQUIRE(path.connections[2].fromFeet == simple_platformer::feetInCell(tests::TileSize, {4, 1}));
    REQUIRE(path.connections[2].toFeet == simple_platformer::feetInCell(tests::TileSize, {4, 3}));
    REQUIRE(path.connections[2].traversal == simple_platformer::Traversal::Fall);
    REQUIRE_FALSE(path.connections[2].completed);
    REQUIRE_FALSE(path.connections[2].next);
}

TEST_CASE("Debug overlay data samples the simulated jump curve", "[app][debug]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const simple_platformer::PlatformerMovementConfig movementConfig;
    const std::vector<simple_platformer::NavigationNeighbor> neighbors =
        simple_platformer::platformerNeighbors(
            map, {2, 2}, {12.0F, 12.0F}, movementConfig, tests::FixedStepSeconds);
    const simple_platformer::NavigationNeighbor& jump =
        tests::neighborWith(neighbors, simple_platformer::Traversal::Jump);

    simple_platformer::Actor npc = tests::ActorBuilder::sized({12.0F, 12.0F})
                                       .at({0.0F, 0.0F})
                                       .walking(movementConfig)
                                       .thinking({});
    npc.pathFollower = simple_platformer::PathFollower{
        simple_platformer::NavigationPath{
            {2, 2}, {{jump.destinationCell, jump.traversal, jump.inputs}}},
        0,
        0.0F,
        jump.destinationCell};

    simple_platformer::World world;
    world.addActor(npc);
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};

    const simple_platformer::DebugOverlay debug = simple_platformer::makeDebugOverlay(
        world, map, cameraController, 128.0F, tests::FixedStepSeconds);
    const simple_platformer::PathFollowerDebugInfo path =
        debug.actors.front().pathFollower.value_or(simple_platformer::PathFollowerDebugInfo{});

    REQUIRE(path.connections.size() == 1);
    REQUIRE(path.connections.front().sampledFeet.size() > 2);
    const float takeoffY = simple_platformer::feetInCell(tests::TileSize, {2, 2}).y;
    const bool risesAboveTakeoff = std::any_of(
        path.connections.front().sampledFeet.begin(),
        path.connections.front().sampledFeet.end(),
        [takeoffY](glm::vec2 feet) { return feet.y < takeoffY; });
    REQUIRE(risesAboveTakeoff);
}

TEST_CASE("The overlay shows only navigation cells near the camera", "[app][debug]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........................", "########################"});
    simple_platformer::World world;
    world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                       .atFeet({8.0F, 16.0F})
                       .walking()
                       .thinking({64.0F, 1.0F}));
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};

    const simple_platformer::DebugOverlay debug = simple_platformer::makeDebugOverlay(
        world, map, cameraController, 128.0F, tests::FixedStepSeconds);

    REQUIRE(debug.navigationCache.has_value());
    const simple_platformer::NavigationCacheDebugInfo navigation =
        debug.navigationCache.value_or(simple_platformer::NavigationCacheDebugInfo{});
    const std::vector<simple_platformer::NavigationCellDebugInfo>& cells = navigation.cells;
    REQUIRE(cells.size() == 21);
    REQUIRE(cells.front().bounds.position == glm::vec2{0.0F, 0.0F});
    // As for actors, one tile beyond the camera's edge is shown; the rest are not.
    REQUIRE(cells.back().bounds.position == glm::vec2{320.0F, 0.0F});
}
