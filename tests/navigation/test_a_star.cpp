#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/a_star.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

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

TEST_CASE("Grid A star finds a shortest path around an obstacle", "[navigation][astar]")
{
    const std::optional<simple_platformer::NavigationPath> path =
        simple_platformer::findGridPath({0, 0}, {3, 0}, openNeighbors);

    REQUIRE(path.has_value());
    const simple_platformer::NavigationPath route =
        path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(route.start == simple_platformer::GridPosition{0, 0});
    REQUIRE(route.steps.back().destination == simple_platformer::GridPosition{3, 0});
    REQUIRE(route.steps.size() == 5);
}

TEST_CASE("Grid A star reports unreachable goals", "[navigation][astar]")
{
    const auto noNeighbors = [](simple_platformer::GridPosition)
    { return std::vector<simple_platformer::NavigationNeighbor>{}; };

    REQUIRE_FALSE(simple_platformer::findGridPath({0, 0}, {1, 0}, noNeighbors));
}

TEST_CASE("Grid A star includes a start that is already the goal", "[navigation][astar]")
{
    const auto noNeighbors = [](simple_platformer::GridPosition)
    { return std::vector<simple_platformer::NavigationNeighbor>{}; };
    const auto path = simple_platformer::findGridPath({2, 3}, {2, 3}, noNeighbors);

    REQUIRE(path.has_value());
    const simple_platformer::NavigationPath route =
        path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(route.start == simple_platformer::GridPosition{2, 3});
    REQUIRE(route.steps.empty());
}

TEST_CASE(
    "Grid A star preserves traversal and chooses lower connection cost",
    "[navigation][astar]")
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

    const auto path = simple_platformer::findGridPath({0, 0}, {2, 0}, neighbors);

    REQUIRE(path.has_value());
    const simple_platformer::NavigationPath route =
        path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(route.steps.size() == 2);
    REQUIRE(route.steps.front().traversal == simple_platformer::Traversal::Walk);
}

TEST_CASE("Grid A star requires a neighbor function", "[navigation][astar]")
{
    REQUIRE_THROWS_AS(simple_platformer::findGridPath({0, 0}, {1, 0}, {}), std::invalid_argument);
}
