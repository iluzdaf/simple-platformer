#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_search.hpp"

namespace
{
    std::vector<simple_platformer::NavigationNeighbor> openNeighbors(
        simple_platformer::GridPosition position)
    {
        std::vector<simple_platformer::NavigationNeighbor> result;
        constexpr simple_platformer::GridPosition Directions[] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        for (const simple_platformer::GridPosition direction : Directions)
        {
            const simple_platformer::GridPosition candidate{
                position.x + direction.x, position.y + direction.y};
            if (candidate.x >= 0 && candidate.x < 4 && candidate.y >= 0 && candidate.y < 3 &&
                candidate != simple_platformer::GridPosition{1, 0})
            {
                result.push_back({candidate, simple_platformer::Traversal::Fly, 1, {}});
            }
        }
        return result;
    }
}

TEST_CASE("Lowest-cost search can use or omit the Manhattan heuristic", "[navigation][path-search]")
{
    const std::optional<simple_platformer::NavigationPath> withoutHeuristic =
        simple_platformer::findLowestCostPath({0, 0}, {3, 0}, openNeighbors);
    const std::optional<simple_platformer::NavigationPath> withHeuristic =
        simple_platformer::findLowestCostPath(
            {0, 0}, {3, 0}, openNeighbors, simple_platformer::manhattanHeuristic);

    REQUIRE(withoutHeuristic.has_value());
    REQUIRE(withHeuristic.has_value());
    const simple_platformer::NavigationPath route =
        withHeuristic.value_or(simple_platformer::NavigationPath{});
    REQUIRE(route.start == simple_platformer::GridPosition{0, 0});
    REQUIRE(route.steps.back().destinationCell == simple_platformer::GridPosition{3, 0});
    REQUIRE(route.steps.size() == 5);
    const simple_platformer::NavigationPath routeWithoutHeuristic =
        withoutHeuristic.value_or(simple_platformer::NavigationPath{});
    REQUIRE(routeWithoutHeuristic.steps.size() == route.steps.size());
}

TEST_CASE("Lowest-cost search reports unreachable goals", "[navigation][path-search]")
{
    const auto noNeighbors = [](simple_platformer::GridPosition)
    { return std::vector<simple_platformer::NavigationNeighbor>{}; };

    REQUIRE_FALSE(simple_platformer::findLowestCostPath({0, 0}, {1, 0}, noNeighbors));
}

TEST_CASE(
    "Lowest-cost search includes a start that is already the goal",
    "[navigation][path-search]")
{
    const auto noNeighbors = [](simple_platformer::GridPosition)
    { return std::vector<simple_platformer::NavigationNeighbor>{}; };
    const auto path = simple_platformer::findLowestCostPath({2, 3}, {2, 3}, noNeighbors);

    REQUIRE(path.has_value());
    const simple_platformer::NavigationPath route =
        path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(route.start == simple_platformer::GridPosition{2, 3});
    REQUIRE(route.steps.empty());
}

TEST_CASE(
    "Lowest-cost search preserves traversal and chooses lower connection cost",
    "[navigation][path-search]")
{
    const auto neighbors = [](simple_platformer::GridPosition position)
    {
        if (position == simple_platformer::GridPosition{0, 0})
        {
            return std::vector<simple_platformer::NavigationNeighbor>{
                {{2, 0}, simple_platformer::Traversal::Jump, 8, {}},
                {{1, 0}, simple_platformer::Traversal::Walk, 1, {}}};
        }
        if (position == simple_platformer::GridPosition{1, 0})
        {
            return std::vector<simple_platformer::NavigationNeighbor>{
                {{2, 0}, simple_platformer::Traversal::Walk, 1, {}}};
        }
        return std::vector<simple_platformer::NavigationNeighbor>{};
    };

    const auto path = simple_platformer::findLowestCostPath({0, 0}, {2, 0}, neighbors);

    REQUIRE(path.has_value());
    const simple_platformer::NavigationPath route =
        path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(route.steps.size() == 2);
    REQUIRE(route.steps.front().traversal == simple_platformer::Traversal::Walk);
}

TEST_CASE(
    "A connection can cross several cells in fewer cost units than its grid distance",
    "[navigation][path-search]")
{
    const auto neighbors = [](simple_platformer::GridPosition position)
    {
        if (position == simple_platformer::GridPosition{0, 0})
        {
            return std::vector<simple_platformer::NavigationNeighbor>{
                {{4, 0}, simple_platformer::Traversal::Jump, 2, {}}};
        }
        return std::vector<simple_platformer::NavigationNeighbor>{};
    };

    const auto path = simple_platformer::findLowestCostPath({0, 0}, {4, 0}, neighbors);

    REQUIRE(path.has_value());
    const simple_platformer::NavigationPath route =
        path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(route.steps.size() == 1);
    REQUIRE(route.steps.front().destinationCell == simple_platformer::GridPosition{4, 0});
}

TEST_CASE("Path search rejects invalid functions and costs", "[navigation][path-search]")
{
    const auto invalidNeighbors = [](simple_platformer::GridPosition)
    {
        return std::vector<simple_platformer::NavigationNeighbor>{
            {{1, 0}, simple_platformer::Traversal::Walk, 0, {}}};
    };
    const simple_platformer::GridHeuristicFunction negativeHeuristic =
        [](simple_platformer::GridPosition, simple_platformer::GridPosition) { return -1; };
    const simple_platformer::GridHeuristicFunction missingHeuristic;

    REQUIRE_THROWS_AS(
        simple_platformer::findLowestCostPath({0, 0}, {1, 0}, {}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findLowestCostPath({0, 0}, {1, 0}, invalidNeighbors),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findLowestCostPath({0, 0}, {1, 0}, openNeighbors, negativeHeuristic),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findLowestCostPath({0, 0}, {1, 0}, openNeighbors, missingHeuristic),
        std::invalid_argument);
}
