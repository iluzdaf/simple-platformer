#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_search.hpp"
#include "simple_platformer/navigation/platformer_connections.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/fixed_step.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"

namespace
{
    using simple_platformer::GridPosition;
    using simple_platformer::NavigationNeighbor;
    using simple_platformer::Traversal;

    constexpr glm::vec2 SmallBody{12.0F, 12.0F};
    constexpr glm::vec2 TallBody{12.0F, 20.0F};

    const NavigationNeighbor& jumpUpFrom(const std::vector<NavigationNeighbor>& neighbors, int row)
    {
        const auto jump = std::find_if(
            neighbors.begin(),
            neighbors.end(),
            [row](const NavigationNeighbor& neighbor)
            { return neighbor.traversal == Traversal::Jump && neighbor.destinationCell.y < row; });
        if (jump == neighbors.end())
        {
            throw std::logic_error("No jump lands above the row");
        }
        return *jump;
    }

    bool hasStep(const simple_platformer::NavigationPath& route, Traversal traversal)
    {
        return std::any_of(
            route.steps.begin(),
            route.steps.end(),
            [traversal](const simple_platformer::NavigationStep& step)
            { return step.traversal == traversal; });
    }
}

TEST_CASE("The tick heuristic is an optimistic horizontal estimate", "[navigation][platformer]")
{
    const simple_platformer::PlatformerMovementConfig movement;
    // Nothing to travel across columns, however far down the goal is.
    REQUIRE(
        simple_platformer::platformerTickHeuristic(
            tests::TileSize, {2, 1}, {2, 8}, movement, tests::FixedStepSeconds) == 0);
    REQUIRE(
        simple_platformer::platformerTickHeuristic(
            tests::TileSize, {2, 1}, {3, 1}, movement, tests::FixedStepSeconds) == 5);

    simple_platformer::PlatformerMovementConfig slower = movement;
    slower.maximumSpeed = movement.maximumSpeed * 0.5F;
    REQUIRE(
        simple_platformer::platformerTickHeuristic(
            tests::TileSize, {2, 1}, {3, 1}, slower, tests::FixedStepSeconds) >
        simple_platformer::platformerTickHeuristic(
            tests::TileSize, {2, 1}, {3, 1}, movement, tests::FixedStepSeconds));

    simple_platformer::PlatformerMovementConfig invalid = movement;
    invalid.maximumSpeed = -1.0F;
    REQUIRE_THROWS_AS(
        simple_platformer::platformerTickHeuristic(
            tests::TileSize, {0, 0}, {1, 0}, invalid, tests::FixedStepSeconds),
        std::invalid_argument);
    invalid.maximumSpeed = std::numeric_limits<float>::infinity();
    REQUIRE_THROWS_AS(
        simple_platformer::platformerTickHeuristic(
            tests::TileSize, {0, 0}, {1, 0}, invalid, tests::FixedStepSeconds),
        std::invalid_argument);
}

TEST_CASE(
    "A platformer search finds the same route with and without its heuristic",
    "[navigation][platformer]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", "#####"});
    const simple_platformer::PlatformerMovementConfig movement;

    const std::optional<simple_platformer::NavigationPath> path =
        simple_platformer::findPlatformerPath(
            map, {1, 0}, {3, 0}, SmallBody, movement, tests::FixedStepSeconds)
            .path;
    REQUIRE(path.has_value());
    const simple_platformer::NavigationPath route =
        path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(route.start == GridPosition{1, 0});
    REQUIRE(route.steps.size() == 1);
    REQUIRE(route.steps.back().destinationCell == GridPosition{3, 0});
    REQUIRE(route.steps.back().traversal == Traversal::Walk);

    const simple_platformer::GridNeighborFunction neighbors =
        [&map,
         &movement](GridPosition position, const simple_platformer::GridNeighborVisitor& visit)
    {
        for (const NavigationNeighbor& neighbor : simple_platformer::platformerNeighbors(
                 map, position, SmallBody, movement, tests::FixedStepSeconds))
        {
            visit(neighbor, neighbor.cost);
        }
    };
    const std::optional<simple_platformer::NavigationPath> pathWithoutHeuristic =
        simple_platformer::findLowestCostPath({1, 0}, {3, 0}, map.size(), neighbors);
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
    "The jump start penalty stops a needless hop but not a needed jump",
    "[navigation][platformer][regression]")
{
    // A platform along the way that a hop over would save a few ticks.
    const simple_platformer::TileMap hop =
        tests::TileMapBuilder({".....###.....", ".............", ".............", "#############"});
    simple_platformer::PlatformerMovementConfig movement;
    movement.maximumSpeed = 60.0F;
    const simple_platformer::NavigationPath preferred =
        simple_platformer::findPlatformerPath(
            hop, {12, 2}, {4, 2}, TallBody, movement, tests::FixedStepSeconds)
            .path.value_or(simple_platformer::NavigationPath{});
    REQUIRE_FALSE(preferred.steps.empty());
    REQUIRE_FALSE(hasStep(preferred, Traversal::Jump));
    simple_platformer::PlatformerNavigationConfig noPenalty;
    noPenalty.jumpStartPenaltyTicks = 0;
    const simple_platformer::NavigationPath fastest =
        simple_platformer::findPlatformerPath(
            hop, {12, 2}, {4, 2}, TallBody, movement, tests::FixedStepSeconds, noPenalty)
            .path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(hasStep(fastest, Traversal::Jump));

    // A goal on a platform is reached only by jumping, penalty or not.
    const simple_platformer::TileMap platform =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const simple_platformer::PlatformerMovementConfig ordinary;
    const std::vector<NavigationNeighbor> neighbors = simple_platformer::platformerNeighbors(
        platform, {2, 2}, SmallBody, ordinary, tests::FixedStepSeconds);
    const NavigationNeighbor& up = jumpUpFrom(neighbors, 2);
    const simple_platformer::NavigationPath climbed =
        simple_platformer::findPlatformerPath(
            platform, {2, 2}, up.destinationCell, SmallBody, ordinary, tests::FixedStepSeconds)
            .path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(hasStep(climbed, Traversal::Jump));
}

TEST_CASE("The tick heuristic uses the caller's step", "[navigation][platformer]")
{
    const simple_platformer::PlatformerMovementConfig movement;
    // Twice the step covers the same distance in half the ticks.
    const int ticks = simple_platformer::platformerTickHeuristic(
        tests::TileSize, {2, 1}, {3, 1}, movement, tests::FixedStepSeconds);
    const int ticksAtDoubleStep = simple_platformer::platformerTickHeuristic(
        tests::TileSize, {2, 1}, {3, 1}, movement, tests::FixedStepSeconds * 2.0F);
    REQUIRE(ticks == 5);
    REQUIRE(ticksAtDoubleStep == 3);
}

TEST_CASE(
    "Platformer search rejects an invalid step or penalty",
    "[navigation][platformer][validation]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"...", "###"});
    const simple_platformer::PlatformerMovementConfig movement;
    for (const float step :
         {0.0F, -tests::FixedStepSeconds, std::numeric_limits<float>::infinity()})
    {
        REQUIRE_THROWS_AS(
            simple_platformer::platformerTickHeuristic(
                tests::TileSize, {0, 0}, {1, 0}, movement, step),
            std::invalid_argument);
        REQUIRE_THROWS_AS(
            simple_platformer::findPlatformerPath(map, {0, 0}, {1, 0}, SmallBody, movement, step),
            std::invalid_argument);
    }

    simple_platformer::PlatformerNavigationConfig negative;
    negative.jumpStartPenaltyTicks = -1;
    REQUIRE_THROWS_AS(
        simple_platformer::findPlatformerPath(
            map, {0, 0}, {1, 0}, SmallBody, movement, tests::FixedStepSeconds, negative),
        std::invalid_argument);
}

TEST_CASE("Platformer searches report what they cost", "[navigation][platformer]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const simple_platformer::PlatformerMovementConfig movement;

    simple_platformer::PathSearchStatistics neighborCost;
    const std::vector<NavigationNeighbor> neighbors = simple_platformer::platformerNeighbors(
        map, {2, 2}, SmallBody, movement, tests::FixedStepSeconds, &neighborCost);
    REQUIRE(!neighbors.empty());
    REQUIRE(neighborCost.nodesExpanded == 0);
    REQUIRE(neighborCost.simulatedTicks > 0);

    simple_platformer::PathSearchStatistics searchCost;
    const std::optional<simple_platformer::NavigationPath> path =
        simple_platformer::findPlatformerPath(
            map, {2, 2}, {7, 2}, SmallBody, movement, tests::FixedStepSeconds, {}, &searchCost)
            .path;
    REQUIRE(path.has_value());
    // The search expands at least its start cell, and simulating that cell's connections
    // is part of what it cost.
    REQUIRE(searchCost.nodesExpanded >= 1);
    REQUIRE(searchCost.simulatedTicks >= neighborCost.simulatedTicks);
}
