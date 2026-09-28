#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_search.hpp"

namespace
{
    using simple_platformer::PathSearchResult;
    using simple_platformer::PathSearchStatus;

    constexpr simple_platformer::GridSize TestGrid{8, 8};

    simple_platformer::GridConnectionFunction connectionsFrom(
        std::vector<std::pair<
            simple_platformer::GridPosition,
            std::vector<simple_platformer::NavigationConnection>>> table)
    {
        return [table = std::move(table)](simple_platformer::GridPosition position)
        {
            for (const auto& [cell, connections] : table)
            {
                if (cell == position)
                {
                    return connections;
                }
            }
            return std::vector<simple_platformer::NavigationConnection>{};
        };
    }

    std::vector<simple_platformer::NavigationConnection> noConnections(
        simple_platformer::GridPosition)
    {
        return {};
    }

    int zeroHeuristic(simple_platformer::GridPosition, simple_platformer::GridPosition)
    {
        return 0;
    }
}

TEST_CASE(
    "A search finds no path to an unreachable goal and an empty path to its start",
    "[navigation][search]")
{
    const PathSearchResult unreachable = simple_platformer::findLowestCostPath(
        {0, 0}, {1, 0}, TestGrid, noConnections, zeroHeuristic);
    REQUIRE(unreachable.status == PathSearchStatus::Unreachable);
    REQUIRE(unreachable.reachableCells == std::vector<simple_platformer::GridPosition>{{0, 0}});

    const auto path = simple_platformer::findLowestCostPath(
        {2, 3}, {2, 3}, TestGrid, noConnections, zeroHeuristic);
    REQUIRE(path.status == PathSearchStatus::Found);
    const simple_platformer::NavigationPath route =
        path.path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(route.start == simple_platformer::GridPosition{2, 3});
    REQUIRE(route.steps.empty());
}

TEST_CASE(
    "A search takes the cheapest connections whatever cells they cross",
    "[navigation][search]")
{
    // A jump straight to the goal costs more than two walks by way of the middle cell.
    const simple_platformer::GridConnectionFunction connections = connectionsFrom(
        {{{0, 0},
          {{{{2, 0}, simple_platformer::Traversal::Jump, {}}, 8},
           {{{1, 0}, simple_platformer::Traversal::Walk, {}}, 1}}},
         {{1, 0}, {{{{2, 0}, simple_platformer::Traversal::Walk, {}}, 1}}}});
    const auto path =
        simple_platformer::findLowestCostPath({0, 0}, {2, 0}, TestGrid, connections, zeroHeuristic);
    REQUIRE(path.status == PathSearchStatus::Found);
    const simple_platformer::NavigationPath route =
        path.path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(route.steps.size() == 2);
    REQUIRE(route.steps.front().traversal == simple_platformer::Traversal::Walk);

    // A connection may cross several cells for less than their number.
    const simple_platformer::GridConnectionFunction leaping =
        connectionsFrom({{{0, 0}, {{{{4, 0}, simple_platformer::Traversal::Jump, {}}, 2}}}});
    const auto leap =
        simple_platformer::findLowestCostPath({0, 0}, {4, 0}, TestGrid, leaping, zeroHeuristic);
    REQUIRE(leap.status == PathSearchStatus::Found);
    const simple_platformer::NavigationPath leapRoute =
        leap.path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(leapRoute.steps.size() == 1);
    REQUIRE(leapRoute.steps.front().destinationCell == simple_platformer::GridPosition{4, 0});
}

TEST_CASE("A failed search reports every reachable cell", "[navigation][search]")
{
    // A line of three cells; nothing leads beyond the last.
    const auto forwardOnly = [](simple_platformer::GridPosition position)
    {
        if (position.x < 2)
        {
            return std::vector<simple_platformer::NavigationConnection>{
                {{{position.x + 1, position.y}, simple_platformer::Traversal::Fly, {}}, 1}};
        }
        return std::vector<simple_platformer::NavigationConnection>{};
    };

    const PathSearchResult none = simple_platformer::findLowestCostPath(
        {0, 0}, {5, 0}, TestGrid, forwardOnly, simple_platformer::manhattanHeuristic);
    REQUIRE(none.status == PathSearchStatus::Unreachable);
    REQUIRE_FALSE(none.path.has_value());
    REQUIRE(
        none.reachableCells ==
        std::vector<simple_platformer::GridPosition>{{0, 0}, {1, 0}, {2, 0}});

    const PathSearchResult found = simple_platformer::findLowestCostPath(
        {0, 0}, {2, 0}, TestGrid, forwardOnly, simple_platformer::manhattanHeuristic);
    REQUIRE(found.status == PathSearchStatus::Found);
    REQUIRE(found.reachableCells.empty());
}

TEST_CASE("A connection policy's costs determine the cheapest path", "[navigation][search]")
{
    // Two ways from the start to the goal: a jump straight there and a walk by way of a
    // middle cell. Changing the jump's cost changes the chosen route.
    const simple_platformer::NavigationConnection jump{
        {{2, 0}, simple_platformer::Traversal::Jump, {{0.5F, {}}}}, 1};
    const simple_platformer::NavigationConnection walkOut{
        {{1, 0}, simple_platformer::Traversal::Walk, {}}, 1};
    const simple_platformer::NavigationConnection walkIn{
        {{2, 0}, simple_platformer::Traversal::Walk, {}}, 1};
    const simple_platformer::GridConnectionFunction expensiveJump =
        [&](simple_platformer::GridPosition cell)
    {
        if (cell == simple_platformer::GridPosition{0, 0})
        {
            simple_platformer::NavigationConnection costlyJump = jump;
            costlyJump.cost += 5;
            return std::vector<simple_platformer::NavigationConnection>{costlyJump, walkOut};
        }
        if (cell == simple_platformer::GridPosition{1, 0})
        {
            return std::vector<simple_platformer::NavigationConnection>{walkIn};
        }
        return std::vector<simple_platformer::NavigationConnection>{};
    };

    const auto path = simple_platformer::findLowestCostPath(
        {0, 0}, {2, 0}, TestGrid, expensiveJump, zeroHeuristic);
    REQUIRE(path.status == PathSearchStatus::Found);
    const simple_platformer::NavigationPath route =
        path.path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(route.steps.size() == 2);
    REQUIRE(route.steps.front().traversal == simple_platformer::Traversal::Walk);

    // At its original cost, the jump wins and its inputs come through.
    const simple_platformer::GridConnectionFunction originalCosts =
        [&](simple_platformer::GridPosition cell)
    {
        if (cell == simple_platformer::GridPosition{0, 0})
        {
            return std::vector<simple_platformer::NavigationConnection>{jump, walkOut};
        }
        return std::vector<simple_platformer::NavigationConnection>{};
    };
    const auto direct = simple_platformer::findLowestCostPath(
        {0, 0}, {2, 0}, TestGrid, originalCosts, zeroHeuristic);
    REQUIRE(direct.status == PathSearchStatus::Found);
    const simple_platformer::NavigationPath directRoute =
        direct.path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(directRoute.steps.size() == 1);
    REQUIRE(directRoute.steps.front().traversal == simple_platformer::Traversal::Jump);
    REQUIRE(directRoute.steps.front().inputs.size() == 1);
}

TEST_CASE(
    "A search rejects cells off its grid, missing functions and costs below one",
    "[navigation][search][validation]")
{
    const auto leadsOut = [](simple_platformer::GridPosition)
    {
        return std::vector<simple_platformer::NavigationConnection>{
            {{{8, 0}, simple_platformer::Traversal::Fly, {}}, 1}};
    };
    REQUIRE_THROWS_AS(
        simple_platformer::findLowestCostPath({0, 0}, {1, 0}, TestGrid, leadsOut, zeroHeuristic),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findLowestCostPath(
            {0, 0}, {9, 0}, TestGrid, noConnections, zeroHeuristic),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findLowestCostPath(
            {-1, 0}, {1, 0}, TestGrid, noConnections, zeroHeuristic),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findLowestCostPath({0, 0}, {1, 0}, {0, 8}, noConnections, zeroHeuristic),
        std::invalid_argument);

    const simple_platformer::GridConnectionFunction missingConnections;
    const simple_platformer::GridHeuristicFunction missingHeuristic;
    const simple_platformer::GridHeuristicFunction negativeHeuristic =
        [](simple_platformer::GridPosition, simple_platformer::GridPosition) { return -1; };
    REQUIRE_THROWS_AS(
        simple_platformer::findLowestCostPath(
            {0, 0}, {1, 0}, TestGrid, missingConnections, zeroHeuristic),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findLowestCostPath(
            {0, 0}, {1, 0}, TestGrid, noConnections, missingHeuristic),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findLowestCostPath(
            {0, 0}, {1, 0}, TestGrid, noConnections, negativeHeuristic),
        std::invalid_argument);

    const auto costsNothing = [](simple_platformer::GridPosition)
    {
        return std::vector<simple_platformer::NavigationConnection>{
            {{{1, 0}, simple_platformer::Traversal::Walk, {}}, 0}};
    };
    REQUIRE_THROWS_AS(
        simple_platformer::findLowestCostPath(
            {0, 0}, {1, 0}, TestGrid, costsNothing, zeroHeuristic),
        std::invalid_argument);
}

TEST_CASE(
    "A pending cell pauses the search before its connections are requested",
    "[navigation][search]")
{
    int connectionQueries = 0;
    const simple_platformer::GridConnectionFunction connections =
        [&](simple_platformer::GridPosition cell)
    {
        ++connectionQueries;
        if (cell == simple_platformer::GridPosition{0, 0})
        {
            return std::vector<simple_platformer::NavigationConnection>{
                {{{1, 0}, simple_platformer::Traversal::Walk, {}}, 1}};
        }
        return std::vector<simple_platformer::NavigationConnection>{};
    };
    const simple_platformer::GridExpansionReady canExpand = [](simple_platformer::GridPosition cell)
    { return cell != simple_platformer::GridPosition{1, 0}; };

    const PathSearchResult result = simple_platformer::findLowestCostPath(
        {0, 0}, {2, 0}, TestGrid, connections, simple_platformer::manhattanHeuristic, canExpand);
    REQUIRE(result.status == PathSearchStatus::Incomplete);
    REQUIRE_FALSE(result.path.has_value());
    REQUIRE(result.unexpandedCell == simple_platformer::GridPosition{1, 0});
    REQUIRE(connectionQueries == 1);
    REQUIRE(result.reachableCells.empty());
}
