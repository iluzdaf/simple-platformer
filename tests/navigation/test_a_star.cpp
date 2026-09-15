#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/a_star.hpp"

namespace
{
    std::vector<simple_platformer::GridPosition> openNeighbors(
        simple_platformer::GridPosition position)
    {
        std::vector<simple_platformer::GridPosition> result;
        constexpr simple_platformer::GridPosition Directions[] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        for (const simple_platformer::GridPosition direction : Directions)
        {
            const simple_platformer::GridPosition candidate{
                position.x + direction.x, position.y + direction.y};
            if (candidate.x >= 0 && candidate.x < 4 && candidate.y >= 0 && candidate.y < 3 &&
                candidate != simple_platformer::GridPosition{1, 0})
            {
                result.push_back(candidate);
            }
        }
        return result;
    }
}

TEST_CASE("Grid A star finds a shortest path around an obstacle", "[navigation][astar]")
{
    const std::optional<std::vector<simple_platformer::GridPosition>> path =
        simple_platformer::findGridPath({0, 0}, {3, 0}, openNeighbors);

    REQUIRE(path.has_value());
    REQUIRE(
        path.value_or(std::vector<simple_platformer::GridPosition>{}).front() ==
        simple_platformer::GridPosition{0, 0});
    REQUIRE(
        path.value_or(std::vector<simple_platformer::GridPosition>{}).back() ==
        simple_platformer::GridPosition{3, 0});
    REQUIRE(path.value_or(std::vector<simple_platformer::GridPosition>{}).size() == 6);
}

TEST_CASE("Grid A star reports unreachable goals", "[navigation][astar]")
{
    const auto noNeighbors = [](simple_platformer::GridPosition)
    { return std::vector<simple_platformer::GridPosition>{}; };

    REQUIRE_FALSE(simple_platformer::findGridPath({0, 0}, {1, 0}, noNeighbors));
}

TEST_CASE("Grid A star includes a start that is already the goal", "[navigation][astar]")
{
    const auto noNeighbors = [](simple_platformer::GridPosition)
    { return std::vector<simple_platformer::GridPosition>{}; };
    const auto path = simple_platformer::findGridPath({2, 3}, {2, 3}, noNeighbors);

    REQUIRE(path.has_value());
    REQUIRE(path.value_or(std::vector<simple_platformer::GridPosition>{}).size() == 1);
}

TEST_CASE("Grid A star requires a neighbor function", "[navigation][astar]")
{
    REQUIRE_THROWS_AS(simple_platformer::findGridPath({0, 0}, {1, 0}, {}), std::invalid_argument);
}
