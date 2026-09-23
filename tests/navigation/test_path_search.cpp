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
    // A 4 by 3 grid open in four directions, with one cell walled off.
    void openNeighbors(
        simple_platformer::GridPosition position,
        const simple_platformer::GridNeighborVisitor& visit)
    {
        constexpr simple_platformer::GridPosition Directions[] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        for (const simple_platformer::GridPosition direction : Directions)
        {
            const simple_platformer::GridPosition candidate{
                position.x + direction.x, position.y + direction.y};
            if (candidate.x >= 0 && candidate.x < 4 && candidate.y >= 0 && candidate.y < 3 &&
                candidate != simple_platformer::GridPosition{1, 0})
            {
                visit({candidate, simple_platformer::Traversal::Fly, 1, {}}, 1);
            }
        }
    }

    // Visits each of these as it is.
    simple_platformer::GridNeighborFunction visiting(
        std::vector<std::pair<
            simple_platformer::GridPosition,
            std::vector<simple_platformer::NavigationNeighbor>>> table)
    {
        return [table = std::move(table)](
                   simple_platformer::GridPosition position,
                   const simple_platformer::GridNeighborVisitor& visit)
        {
            for (const auto& [cell, neighbors] : table)
            {
                if (cell != position)
                {
                    continue;
                }
                for (const simple_platformer::NavigationNeighbor& neighbor : neighbors)
                {
                    visit(neighbor, neighbor.cost);
                }
            }
        };
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
    const auto noNeighbors = [](simple_platformer::GridPosition,
                                const simple_platformer::GridNeighborVisitor&) {};

    REQUIRE_FALSE(simple_platformer::findLowestCostPath({0, 0}, {1, 0}, noNeighbors));
}

TEST_CASE(
    "Lowest-cost search includes a start that is already the goal",
    "[navigation][path-search]")
{
    const auto noNeighbors = [](simple_platformer::GridPosition,
                                const simple_platformer::GridNeighborVisitor&) {};
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
    const simple_platformer::GridNeighborFunction neighbors = visiting(
        {{{0, 0},
          {{{2, 0}, simple_platformer::Traversal::Jump, 8, {}},
           {{1, 0}, simple_platformer::Traversal::Walk, 1, {}}}},
         {{1, 0}, {{{2, 0}, simple_platformer::Traversal::Walk, 1, {}}}}});

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
    const simple_platformer::GridNeighborFunction neighbors =
        visiting({{{0, 0}, {{{4, 0}, simple_platformer::Traversal::Jump, 2, {}}}}});

    const auto path = simple_platformer::findLowestCostPath({0, 0}, {4, 0}, neighbors);

    REQUIRE(path.has_value());
    const simple_platformer::NavigationPath route =
        path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(route.steps.size() == 1);
    REQUIRE(route.steps.front().destinationCell == simple_platformer::GridPosition{4, 0});
}

TEST_CASE("A search with no path reports every cell it reached", "[navigation][path-search]")
{
    // A line of three cells; nothing leads beyond the last.
    const auto forwardOnly = [](simple_platformer::GridPosition position,
                                const simple_platformer::GridNeighborVisitor& visit)
    {
        if (position.x < 2)
        {
            visit({{position.x + 1, position.y}, simple_platformer::Traversal::Fly, 1, {}}, 1);
        }
    };
    std::vector<simple_platformer::GridPosition> reached{{9, 9}};

    const std::optional<simple_platformer::NavigationPath> none =
        simple_platformer::findLowestCostPath(
            {0, 0}, {5, 0}, forwardOnly, simple_platformer::manhattanHeuristic, nullptr, &reached);
    REQUIRE_FALSE(none.has_value());
    REQUIRE(reached == std::vector<simple_platformer::GridPosition>{{0, 0}, {1, 0}, {2, 0}});

    // A search that finds its goal leaves what was passed alone.
    std::vector<simple_platformer::GridPosition> untouched{{9, 9}};
    const std::optional<simple_platformer::NavigationPath> found =
        simple_platformer::findLowestCostPath(
            {0, 0},
            {2, 0},
            forwardOnly,
            simple_platformer::manhattanHeuristic,
            nullptr,
            &untouched);
    REQUIRE(found.has_value());
    REQUIRE(untouched == std::vector<simple_platformer::GridPosition>{{9, 9}});
}

TEST_CASE("Connections visited in place are charged the cost given", "[navigation][path-search]")
{
    // Two ways from the start to the goal: a jump straight there and a walk by way of a
    // middle cell. The jump's own cost is lower, but the visit charges it more.
    const simple_platformer::NavigationNeighbor jump{
        {2, 0}, simple_platformer::Traversal::Jump, 1, {{0.5F, {}}}};
    const simple_platformer::NavigationNeighbor walkOut{
        {1, 0}, simple_platformer::Traversal::Walk, 1, {}};
    const simple_platformer::NavigationNeighbor walkIn{
        {2, 0}, simple_platformer::Traversal::Walk, 1, {}};
    int visits = 0;
    const simple_platformer::GridNeighborFunction visitNeighbors =
        [&](simple_platformer::GridPosition cell,
            const simple_platformer::GridNeighborVisitor& visit)
    {
        ++visits;
        if (cell == simple_platformer::GridPosition{0, 0})
        {
            visit(jump, jump.cost + 5);
            visit(walkOut, walkOut.cost);
        }
        else if (cell == simple_platformer::GridPosition{1, 0})
        {
            visit(walkIn, walkIn.cost);
        }
    };

    simple_platformer::PathSearchStatistics statistics;
    const auto path = simple_platformer::findLowestCostPath(
        {0, 0}, {2, 0}, visitNeighbors, simple_platformer::manhattanHeuristic, &statistics);
    REQUIRE(path.has_value());
    const simple_platformer::NavigationPath route =
        path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(route.steps.size() == 2);
    REQUIRE(route.steps.front().traversal == simple_platformer::Traversal::Walk);
    REQUIRE(statistics.nodesExpanded == visits);

    // Charged the same as its own cost, the jump wins and its inputs come through.
    const simple_platformer::GridNeighborFunction visitPlainly =
        [&](simple_platformer::GridPosition cell,
            const simple_platformer::GridNeighborVisitor& visit)
    {
        if (cell == simple_platformer::GridPosition{0, 0})
        {
            visit(jump, jump.cost);
            visit(walkOut, walkOut.cost);
        }
    };
    const auto direct = simple_platformer::findLowestCostPath(
        {0, 0}, {2, 0}, visitPlainly, simple_platformer::manhattanHeuristic);
    REQUIRE(direct.has_value());
    const simple_platformer::NavigationPath directRoute =
        direct.value_or(simple_platformer::NavigationPath{});
    REQUIRE(directRoute.steps.size() == 1);
    REQUIRE(directRoute.steps.front().traversal == simple_platformer::Traversal::Jump);
    REQUIRE(directRoute.steps.front().inputs.size() == 1);

    const simple_platformer::GridNeighborFunction chargesNothing =
        [&](simple_platformer::GridPosition, const simple_platformer::GridNeighborVisitor& visit)
    { visit(walkOut, 0); };
    REQUIRE_THROWS_AS(
        simple_platformer::findLowestCostPath(
            {0, 0}, {1, 0}, chargesNothing, simple_platformer::manhattanHeuristic),
        std::invalid_argument);
    const simple_platformer::GridNeighborFunction missing;
    REQUIRE_THROWS_AS(
        simple_platformer::findLowestCostPath(
            {0, 0}, {1, 0}, missing, simple_platformer::manhattanHeuristic),
        std::invalid_argument);
}

TEST_CASE("Path search rejects invalid functions and costs", "[navigation][path-search]")
{
    const auto invalidNeighbors =
        [](simple_platformer::GridPosition, const simple_platformer::GridNeighborVisitor& visit)
    { visit({{1, 0}, simple_platformer::Traversal::Walk, 0, {}}, 0); };
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
