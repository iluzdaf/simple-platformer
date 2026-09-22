#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/input_program.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/path_search.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"
#include "support/fixed_step.hpp"
#include "support/neighbor_with.hpp"

TEST_CASE("Standable cells require support and body clearance", "[navigation][platformer]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".#.", "...", "###"});

    REQUIRE(simple_platformer::canStandAt(map, {1, 1}, {12.0F, 12.0F}));
    REQUIRE_FALSE(simple_platformer::canStandAt(map, {1, 1}, {12.0F, 20.0F}));
    REQUIRE_FALSE(simple_platformer::canStandAt(map, {1, 0}, {12.0F, 12.0F}));
}

TEST_CASE(
    "A platformer start cell comes from the collider support at a ledge",
    "[navigation][platformer][regression]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "..###..."});
    simple_platformer::Aabb bounds{{0.0F, 0.0F}, {12.0F, 20.0F}};
    simple_platformer::placeFeetAt(bounds, {80.5F, 32.0F});

    REQUIRE(
        simple_platformer::cellAtFeet(tests::TileSize, simple_platformer::feetOf(bounds)) ==
        simple_platformer::GridPosition{5, 1});
    REQUIRE(
        simple_platformer::findPlatformerStartCell(map, bounds) ==
        simple_platformer::GridPosition{4, 1});
}

TEST_CASE(
    "A supported collider uses its ordinary platformer start cell",
    "[navigation][platformer][exercise]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", "....", "####"});
    simple_platformer::Aabb bounds =
        simple_platformer::boxInCell(tests::TileSize, {1, 1}, {12.0F, 20.0F});

    REQUIRE(
        simple_platformer::findPlatformerStartCell(map, bounds) ==
        simple_platformer::GridPosition{1, 1});
}

TEST_CASE(
    "An unsupported collider has no platformer start cell",
    "[navigation][platformer][exercise]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", "....", "...."});
    simple_platformer::Aabb bounds =
        simple_platformer::boxInCell(tests::TileSize, {1, 1}, {12.0F, 20.0F});

    REQUIRE(simple_platformer::findPlatformerStartCell(map, bounds) == std::nullopt);
}

TEST_CASE(
    "A platformer start cell supports bodies wider than one tile",
    "[navigation][platformer][exercise]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", "....", "####"});
    simple_platformer::Aabb bounds =
        simple_platformer::boxInCell(tests::TileSize, {1, 1}, {20.0F, 20.0F});

    REQUIRE(
        simple_platformer::findPlatformerStartCell(map, bounds) ==
        simple_platformer::GridPosition{1, 1});
}

TEST_CASE("Platformer start cells reject invalid bounds", "[navigation][platformer][exercise]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", "....", "####"});
    const simple_platformer::Aabb bounds{{0.0F, 0.0F}, {0.0F, 20.0F}};

    REQUIRE_THROWS_AS(
        simple_platformer::findPlatformerStartCell(map, bounds), std::invalid_argument);
}

TEST_CASE("A platformer chase keeps an already standable target cell", "[navigation][platformer]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(map, {47.5F, 32.0F}, {12.0F, 20.0F}) ==
        simple_platformer::GridPosition{2, 1});
}

TEST_CASE("Platformer chase destinations handle either platform edge", "[navigation][platformer]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "..###...", "........", "########"});
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(map, {31.5F, 32.0F}, {12.0F, 20.0F}) ==
        simple_platformer::GridPosition{2, 1});
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(map, {80.5F, 32.0F}, {12.0F, 20.0F}) ==
        simple_platformer::GridPosition{4, 1});
}

TEST_CASE("An airborne chase target selects the closest standable feet", "[navigation][platformer]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "..###...", "........", "########"});
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(map, {31.0F, 20.0F}, {12.0F, 20.0F}) ==
        simple_platformer::GridPosition{2, 1});
}

TEST_CASE("Chase destinations use the pursuing body size", "[navigation][platformer]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"..#....", ".......", "#######"});
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(map, {40.0F, 32.0F}, {12.0F, 12.0F}) ==
        simple_platformer::GridPosition{2, 1});
    // The taller NPC cannot fit under the ceiling. Equal-distance alternatives
    // use row, then column order, so the cell on the left wins the tie.
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(map, {40.0F, 32.0F}, {12.0F, 20.0F}) ==
        simple_platformer::GridPosition{1, 1});
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(map, {8.0F, 32.0F}, {20.0F, 12.0F}) ==
        simple_platformer::GridPosition{1, 1});
}

TEST_CASE(
    "Chase destinations handle missing support and outside targets",
    "[navigation][platformer]")
{
    const simple_platformer::TileMap blocked = tests::TileMapBuilder({"###", "###"});
    REQUIRE_FALSE(
        simple_platformer::findPlatformerChaseCell(blocked, {24.0F, 16.0F}, {12.0F, 20.0F}));
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(map, {-16.0F, 32.0F}, {12.0F, 20.0F}) ==
        simple_platformer::GridPosition{0, 1});
}

TEST_CASE("Chase destinations reject invalid inputs", "[navigation][platformer]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    REQUIRE_THROWS_AS(
        simple_platformer::findPlatformerChaseCell(map, {infinity, 32.0F}, {12.0F, 20.0F}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findPlatformerChaseCell(map, {24.0F, nan}, {12.0F, 20.0F}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findPlatformerChaseCell(map, {24.0F, 32.0F}, {0.0F, 20.0F}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findPlatformerChaseCell(map, {24.0F, 32.0F}, {12.0F, -1.0F}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findPlatformerChaseCell(map, {24.0F, 32.0F}, {infinity, 20.0F}),
        std::invalid_argument);
}

TEST_CASE(
    "Platformer tick heuristic is an optimistic horizontal estimate",
    "[navigation][platformer]")
{
    const simple_platformer::PlatformerMovementConfig movement;
    REQUIRE(
        simple_platformer::platformerTickHeuristic(tests::TileSize, {2, 1}, {2, 8}, movement) == 0);
    REQUIRE(
        simple_platformer::platformerTickHeuristic(tests::TileSize, {2, 1}, {3, 1}, movement) == 5);

    simple_platformer::PlatformerMovementConfig slower = movement;
    slower.maximumSpeed = movement.maximumSpeed * 0.5F;
    REQUIRE(
        simple_platformer::platformerTickHeuristic(tests::TileSize, {2, 1}, {3, 1}, slower) >
        simple_platformer::platformerTickHeuristic(tests::TileSize, {2, 1}, {3, 1}, movement));

    simple_platformer::PlatformerMovementConfig invalid = movement;
    invalid.maximumSpeed = -1.0F;
    REQUIRE_THROWS_AS(
        simple_platformer::platformerTickHeuristic(tests::TileSize, {0, 0}, {1, 0}, invalid),
        std::invalid_argument);

    invalid.maximumSpeed = std::numeric_limits<float>::infinity();
    REQUIRE_THROWS_AS(
        simple_platformer::platformerTickHeuristic(tests::TileSize, {0, 0}, {1, 0}, invalid),
        std::invalid_argument);
}

TEST_CASE("Platformer neighbors include walks and simulated falls", "[navigation][platformer]")
{
    const simple_platformer::TileMap floor = tests::TileMapBuilder({"....", "####"});
    const auto walks = simple_platformer::platformerNeighbors(
        floor, {1, 0}, {12.0F, 12.0F}, simple_platformer::PlatformerMovementConfig{});
    REQUIRE(
        std::count_if(
            walks.begin(),
            walks.end(),
            [](const simple_platformer::NavigationNeighbor& neighbor)
            { return neighbor.traversal == simple_platformer::Traversal::Walk; }) == 3);

    const simple_platformer::TileMap ledge =
        tests::TileMapBuilder({"........", "###.....", "........", "........", "########"});
    const auto falls = simple_platformer::platformerNeighbors(
        ledge, {2, 0}, {12.0F, 12.0F}, simple_platformer::PlatformerMovementConfig{});
    const simple_platformer::NavigationNeighbor& fall =
        tests::neighborWith(falls, simple_platformer::Traversal::Fall);
    REQUIRE(fall.destinationCell.y > 0);
    REQUIRE_FALSE(fall.inputs.empty());
}

TEST_CASE("Platformer neighbors include continuous multi-cell walks", "[navigation][platformer]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"......", "######"});
    const simple_platformer::PlatformerMovementConfig movement;

    const auto neighbors =
        simple_platformer::platformerNeighbors(map, {1, 0}, {12.0F, 12.0F}, movement);
    const auto directWalk = std::find_if(
        neighbors.begin(),
        neighbors.end(),
        [](const simple_platformer::NavigationNeighbor& neighbor)
        {
            return neighbor.destinationCell == simple_platformer::GridPosition{3, 0} &&
                   neighbor.traversal == simple_platformer::Traversal::Walk;
        });
    REQUIRE(directWalk != neighbors.end());

    const auto firstWalk = std::find_if(
        neighbors.begin(),
        neighbors.end(),
        [](const simple_platformer::NavigationNeighbor& neighbor)
        {
            return neighbor.destinationCell == simple_platformer::GridPosition{2, 0} &&
                   neighbor.traversal == simple_platformer::Traversal::Walk;
        });
    REQUIRE(firstWalk != neighbors.end());
    const auto nextNeighbors = simple_platformer::platformerNeighbors(
        map, firstWalk->destinationCell, {12.0F, 12.0F}, movement);
    const auto secondWalk = std::find_if(
        nextNeighbors.begin(),
        nextNeighbors.end(),
        [](const simple_platformer::NavigationNeighbor& neighbor)
        {
            return neighbor.destinationCell == simple_platformer::GridPosition{3, 0} &&
                   neighbor.traversal == simple_platformer::Traversal::Walk;
        });
    REQUIRE(secondWalk != nextNeighbors.end());
    REQUIRE(directWalk->cost < firstWalk->cost + secondWalk->cost);
}

TEST_CASE("Generated jump inputs replay to their promised landing", "[navigation][platformer]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const simple_platformer::PlatformerMovementConfig config;
    const auto neighbors =
        simple_platformer::platformerNeighbors(map, {2, 2}, {12.0F, 12.0F}, config);
    const simple_platformer::NavigationNeighbor& jump =
        tests::neighborWith(neighbors, simple_platformer::Traversal::Jump);

    simple_platformer::Body body{
        simple_platformer::boxInCell(tests::TileSize, {2, 2}, {12.0F, 12.0F}), {0.0F, 0.0F}};
    simple_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};
    const float duration = simple_platformer::durationOf(jump.inputs);
    const long tickCount = std::lround(duration / tests::FixedStepSeconds);
    for (int tick = 0; tick < tickCount; ++tick)
    {
        const float elapsed = static_cast<float>(tick) * tests::FixedStepSeconds;
        const simple_platformer::InputIntentions intentions =
            simple_platformer::replayInput(jump.inputs, elapsed);
        simple_platformer::updatePlatformerMovement(
            map, body, movement, intentions, tests::FixedStepSeconds);
    }

    REQUIRE(movement.grounded);
    REQUIRE(
        simple_platformer::cellAtFeet(tests::TileSize, simple_platformer::feetOf(body.bounds)) ==
        jump.destinationCell);
}

TEST_CASE(
    "Platformer connection costs equal their fixed movement ticks",
    "[navigation][platformer]")
{
    const simple_platformer::PlatformerMovementConfig config;

    const simple_platformer::TileMap walkMap = tests::TileMapBuilder({"....", "####"});
    const auto walkNeighbors =
        simple_platformer::platformerNeighbors(walkMap, {1, 0}, {12.0F, 12.0F}, config);
    const simple_platformer::NavigationNeighbor& walk =
        tests::neighborWith(walkNeighbors, simple_platformer::Traversal::Walk);
    simple_platformer::PathFollower follower;
    simple_platformer::setPath(
        follower, {{1, 0}, {{walk.destinationCell, walk.traversal, {}}}}, walk.destinationCell);
    simple_platformer::Body body{
        simple_platformer::boxInCell(tests::TileSize, {1, 0}, {12.0F, 12.0F}), {0.0F, 0.0F}};
    simple_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};
    int walkTicks = 0;
    while (walkTicks < 120 && !simple_platformer::pathComplete(follower))
    {
        const simple_platformer::InputIntentions intentions =
            simple_platformer::followPlatformerPath(
                tests::TileSize, body, movement, follower, tests::FixedStepSeconds);
        if (!simple_platformer::pathComplete(follower))
        {
            simple_platformer::updatePlatformerMovement(
                walkMap, body, movement, intentions, tests::FixedStepSeconds);
            ++walkTicks;
        }
    }
    REQUIRE(simple_platformer::pathComplete(follower));
    REQUIRE(walk.cost == walkTicks);
    REQUIRE(
        simple_platformer::platformerTickHeuristic(
            tests::TileSize, {1, 0}, walk.destinationCell, config) <= walk.cost);

    const simple_platformer::TileMap jumpMap =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const auto jumpNeighbors =
        simple_platformer::platformerNeighbors(jumpMap, {2, 2}, {12.0F, 12.0F}, config);
    const simple_platformer::NavigationNeighbor& jump =
        tests::neighborWith(jumpNeighbors, simple_platformer::Traversal::Jump);
    REQUIRE(
        jump.cost ==
        std::lround(simple_platformer::durationOf(jump.inputs) / tests::FixedStepSeconds));
    REQUIRE(
        simple_platformer::platformerTickHeuristic(
            tests::TileSize, {2, 2}, jump.destinationCell, config) <= jump.cost);

    const simple_platformer::TileMap fallMap =
        tests::TileMapBuilder({"........", "###.....", "........", "........", "########"});
    const auto fallNeighbors =
        simple_platformer::platformerNeighbors(fallMap, {2, 0}, {12.0F, 12.0F}, config);
    const simple_platformer::NavigationNeighbor& fall =
        tests::neighborWith(fallNeighbors, simple_platformer::Traversal::Fall);
    REQUIRE(
        fall.cost ==
        std::lround(simple_platformer::durationOf(fall.inputs) / tests::FixedStepSeconds));
    REQUIRE(
        simple_platformer::platformerTickHeuristic(
            tests::TileSize, {2, 0}, fall.destinationCell, config) <= fall.cost);
}

TEST_CASE(
    "Platformer path search uses the platformer navigation policy",
    "[navigation][platformer]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", "#####"});

    const simple_platformer::PlatformerMovementConfig movement;
    const auto path =
        simple_platformer::findPlatformerPath(map, {1, 0}, {3, 0}, {12.0F, 12.0F}, movement);

    REQUIRE(path.has_value());
    const simple_platformer::NavigationPath route =
        path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(route.start == simple_platformer::GridPosition{1, 0});
    REQUIRE(route.steps.size() == 1);
    REQUIRE(route.steps.back().destinationCell == simple_platformer::GridPosition{3, 0});
    REQUIRE(route.steps.back().traversal == simple_platformer::Traversal::Walk);

    const simple_platformer::GridNeighborFunction neighbors =
        [&map, &movement](simple_platformer::GridPosition position)
    { return simple_platformer::platformerNeighbors(map, position, {12.0F, 12.0F}, movement); };
    const auto pathWithoutHeuristic =
        simple_platformer::findLowestCostPath({1, 0}, {3, 0}, neighbors);
    REQUIRE(pathWithoutHeuristic.has_value());
    const simple_platformer::NavigationPath routeWithoutHeuristic =
        pathWithoutHeuristic.value_or(simple_platformer::NavigationPath{});
    REQUIRE(routeWithoutHeuristic.steps.size() == route.steps.size());
    for (std::size_t index = 0; index < route.steps.size(); ++index)
    {
        REQUIRE(
            routeWithoutHeuristic.steps[index].destinationCell ==
            route.steps[index].destinationCell);
        REQUIRE(routeWithoutHeuristic.steps[index].traversal == route.steps[index].traversal);
    }
}

TEST_CASE(
    "A jump start penalty prevents an unnecessary same-platform hop",
    "[navigation][platformer][regression]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({".....###.....", ".............", ".............", "#############"});
    simple_platformer::PlatformerMovementConfig movement;
    movement.maximumSpeed = 60.0F;

    const auto preferredPath =
        simple_platformer::findPlatformerPath(map, {12, 2}, {4, 2}, {12.0F, 20.0F}, movement);

    REQUIRE(preferredPath.has_value());
    const simple_platformer::NavigationPath preferredRoute =
        preferredPath.value_or(simple_platformer::NavigationPath{});
    REQUIRE(std::all_of(
        preferredRoute.steps.begin(),
        preferredRoute.steps.end(),
        [](const simple_platformer::NavigationStep& step)
        { return step.traversal == simple_platformer::Traversal::Walk; }));

    simple_platformer::PlatformerNavigationConfig noPenalty;
    noPenalty.jumpStartPenaltyTicks = 0;
    const auto fastestPath = simple_platformer::findPlatformerPath(
        map, {12, 2}, {4, 2}, {12.0F, 20.0F}, movement, noPenalty);

    REQUIRE(fastestPath.has_value());
    const simple_platformer::NavigationPath fastestRoute =
        fastestPath.value_or(simple_platformer::NavigationPath{});
    REQUIRE(std::any_of(
        fastestRoute.steps.begin(),
        fastestRoute.steps.end(),
        [](const simple_platformer::NavigationStep& step)
        { return step.traversal == simple_platformer::Traversal::Jump; }));
}

TEST_CASE("A jump start penalty preserves required jumps", "[navigation][platformer]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const simple_platformer::PlatformerMovementConfig movement;
    const auto neighbors =
        simple_platformer::platformerNeighbors(map, {2, 2}, {12.0F, 12.0F}, movement);
    const auto upwardJump = std::find_if(
        neighbors.begin(),
        neighbors.end(),
        [](const simple_platformer::NavigationNeighbor& neighbor)
        {
            return neighbor.traversal == simple_platformer::Traversal::Jump &&
                   neighbor.destinationCell.y < 2;
        });
    REQUIRE(upwardJump != neighbors.end());

    const auto path = simple_platformer::findPlatformerPath(
        map, {2, 2}, upwardJump->destinationCell, {12.0F, 12.0F}, movement);

    REQUIRE(path.has_value());
    const simple_platformer::NavigationPath route =
        path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(std::any_of(
        route.steps.begin(),
        route.steps.end(),
        [](const simple_platformer::NavigationStep& step)
        { return step.traversal == simple_platformer::Traversal::Jump; }));
}

TEST_CASE("Platformer paths reject a negative jump start penalty", "[navigation][platformer]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"...", "###"});
    simple_platformer::PlatformerNavigationConfig navigation;
    navigation.jumpStartPenaltyTicks = -1;

    REQUIRE_THROWS_AS(
        simple_platformer::findPlatformerPath(
            map,
            {0, 0},
            {1, 0},
            {12.0F, 12.0F},
            simple_platformer::PlatformerMovementConfig{},
            navigation),
        std::invalid_argument);
}
