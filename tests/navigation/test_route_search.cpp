#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/navigation/route.hpp"
#include "simple_platformer/navigation/route_search.hpp"
#include "simple_platformer/navigation/traversal.hpp"

namespace
{
    using simple_platformer::Cell;
    using simple_platformer::RouteLocation;
    using simple_platformer::RouteSearchResult;

    constexpr simple_platformer::GridSize TestGrid{8, 8};

    simple_platformer::ConnectionFunction connectionsFrom(
        std::vector<std::pair<Cell, std::vector<simple_platformer::RouteConnection>>> table)
    {
        return [table = std::move(table)](RouteLocation location)
        {
            for (const auto& [cell, connections] : table)
            {
                if (cell == location.cell)
                {
                    return connections;
                }
            }
            return std::vector<simple_platformer::RouteConnection>{};
        };
    }

    std::vector<simple_platformer::RouteConnection> noConnections(RouteLocation)
    {
        return {};
    }

    int zeroHeuristic(Cell, Cell)
    {
        return 0;
    }

    // An admissible estimate for cost-one steps between neighbouring cells.
    int gridSteps(Cell cell, Cell goal)
    {
        return std::abs(cell.x - goal.x) + std::abs(cell.y - goal.y);
    }

    simple_platformer::Route routeOf(const RouteSearchResult& result)
    {
        return result.route.value_or(simple_platformer::Route{});
    }

    // The floor of the cell, the only location a policy without climbing uses.
    RouteLocation floorOf(int x, int y)
    {
        return {{x, y}};
    }
}

TEST_CASE(
    "A search stays put when nothing is reachable and has an empty path to its start",
    "[navigation][search]")
{
    const RouteSearchResult unreachable = simple_platformer::findLowestCostRoute(
        floorOf(0, 0), floorOf(1, 0).cell, TestGrid, noConnections, zeroHeuristic);
    REQUIRE(unreachable.route.has_value());
    REQUIRE(routeOf(unreachable).steps.empty());

    const auto path = simple_platformer::findLowestCostRoute(
        floorOf(2, 3), floorOf(2, 3).cell, TestGrid, noConnections, zeroHeuristic);
    REQUIRE(path.route.has_value());
    const simple_platformer::Route route = routeOf(path);
    REQUIRE(route.start.cell == Cell{2, 3});
    REQUIRE(route.steps.empty());
}

TEST_CASE("Search keeps different attachments in one cell distinct", "[navigation][search]")
{
    using simple_platformer::ClimbSurface;
    using simple_platformer::RouteConnection;
    using simple_platformer::Traversal;

    // Only the ceiling of the first cell leads on to the goal cell. The ceiling is
    // cheaper by way of the wall than straight from the floor.
    const simple_platformer::ConnectionFunction connections = [](RouteLocation location)
    {
        switch (location.surface)
        {
        case ClimbSurface::None:
            return std::vector<RouteConnection>{
                {{{{0, 0}, ClimbSurface::Ceiling}, Traversal::Climb, {}}, 5},
                {{{{0, 0}, ClimbSurface::LeftWall}, Traversal::Climb, {}}, 1}};
        case ClimbSurface::LeftWall:
            return std::vector<RouteConnection>{
                {{{{0, 0}, ClimbSurface::Ceiling}, Traversal::Climb, {}}, 1}};
        case ClimbSurface::Ceiling:
            return std::vector<RouteConnection>{
                {{{{1, 0}, ClimbSurface::Ceiling}, Traversal::Climb, {}}, 1}};
        case ClimbSurface::RightWall:
            break;
        }
        return std::vector<RouteConnection>{};
    };
    const RouteSearchResult result = simple_platformer::findLowestCostRoute(
        RouteLocation{{0, 0}, ClimbSurface::None}, {1, 0}, {2, 1}, connections, zeroHeuristic);
    REQUIRE(result.route.has_value());
    REQUIRE(routeOf(result).steps.size() == 3);
    REQUIRE(routeOf(result).steps[0].destination.surface == ClimbSurface::LeftWall);
    REQUIRE(routeOf(result).steps[1].destination.surface == ClimbSurface::Ceiling);
    REQUIRE(routeOf(result).steps[2].destination.cell == Cell{1, 0});
}

TEST_CASE(
    "A search takes the cheapest connections whatever cells they cross",
    "[navigation][search]")
{
    // A jump straight to the goal costs more than two walks by way of the middle cell.
    const simple_platformer::ConnectionFunction connections = connectionsFrom(
        {{{0, 0},
          {{{{{2, 0}}, simple_platformer::Traversal::Jump, {}}, 8},
           {{{{1, 0}}, simple_platformer::Traversal::Walk, {}}, 1}}},
         {{1, 0}, {{{{{2, 0}}, simple_platformer::Traversal::Walk, {}}, 1}}}});
    const auto path = simple_platformer::findLowestCostRoute(
        floorOf(0, 0), floorOf(2, 0).cell, TestGrid, connections, zeroHeuristic);
    REQUIRE(path.route.has_value());
    const simple_platformer::Route route = routeOf(path);
    REQUIRE(route.steps.size() == 2);
    REQUIRE(route.steps.front().traversal == simple_platformer::Traversal::Walk);

    // A connection may cross several cells for less than their number.
    const simple_platformer::ConnectionFunction leaping =
        connectionsFrom({{{0, 0}, {{{{{4, 0}}, simple_platformer::Traversal::Jump, {}}, 2}}}});
    const auto leap = simple_platformer::findLowestCostRoute(
        floorOf(0, 0), floorOf(4, 0).cell, TestGrid, leaping, zeroHeuristic);
    REQUIRE(leap.route.has_value());
    const simple_platformer::Route leapRoute = routeOf(leap);
    REQUIRE(leapRoute.steps.size() == 1);
    REQUIRE(leapRoute.steps.front().destination.cell == Cell{4, 0});
}

TEST_CASE("A failed search leads to the closest reachable location", "[navigation][search]")
{
    // A line of three cells; nothing leads beyond the last.
    const auto forwardOnly = [](RouteLocation location)
    {
        const Cell position = location.cell;
        if (position.x < 2)
        {
            return std::vector<simple_platformer::RouteConnection>{
                {{{{position.x + 1, position.y}}, simple_platformer::Traversal::Fly, {}}, 1}};
        }
        return std::vector<simple_platformer::RouteConnection>{};
    };

    const RouteSearchResult none = simple_platformer::findLowestCostRoute(
        floorOf(0, 0), floorOf(5, 0).cell, TestGrid, forwardOnly, gridSteps);
    REQUIRE(none.route.has_value());
    REQUIRE(simple_platformer::endOf(routeOf(none)) == floorOf(2, 0));
    REQUIRE(routeOf(none).steps.size() == 2);

    const RouteSearchResult found = simple_platformer::findLowestCostRoute(
        floorOf(0, 0), floorOf(2, 0).cell, TestGrid, forwardOnly, gridSteps);
    REQUIRE(found.route.has_value());
}

TEST_CASE("A connection policy's costs determine the cheapest path", "[navigation][search]")
{
    // Two ways from the start to the goal: a jump straight there and a walk by way of a
    // middle cell. Changing the jump's cost changes the chosen route.
    const simple_platformer::RouteConnection jump{
        {{{2, 0}}, simple_platformer::Traversal::Jump, {{0.5F, {}}}}, 1};
    const simple_platformer::RouteConnection walkOut{
        {{{1, 0}}, simple_platformer::Traversal::Walk, {}}, 1};
    const simple_platformer::RouteConnection walkIn{
        {{{2, 0}}, simple_platformer::Traversal::Walk, {}}, 1};
    const simple_platformer::ConnectionFunction expensiveJump = [&](RouteLocation location)
    {
        if (location.cell == Cell{0, 0})
        {
            simple_platformer::RouteConnection costlyJump = jump;
            costlyJump.cost += 5;
            return std::vector<simple_platformer::RouteConnection>{costlyJump, walkOut};
        }
        if (location.cell == Cell{1, 0})
        {
            return std::vector<simple_platformer::RouteConnection>{walkIn};
        }
        return std::vector<simple_platformer::RouteConnection>{};
    };

    const auto path = simple_platformer::findLowestCostRoute(
        floorOf(0, 0), floorOf(2, 0).cell, TestGrid, expensiveJump, zeroHeuristic);
    REQUIRE(path.route.has_value());
    const simple_platformer::Route route = routeOf(path);
    REQUIRE(route.steps.size() == 2);
    REQUIRE(route.steps.front().traversal == simple_platformer::Traversal::Walk);

    // At its original cost, the jump wins and its inputs come through.
    const simple_platformer::ConnectionFunction originalCosts = [&](RouteLocation location)
    {
        if (location.cell == Cell{0, 0})
        {
            return std::vector<simple_platformer::RouteConnection>{jump, walkOut};
        }
        return std::vector<simple_platformer::RouteConnection>{};
    };
    const auto direct = simple_platformer::findLowestCostRoute(
        floorOf(0, 0), floorOf(2, 0).cell, TestGrid, originalCosts, zeroHeuristic);
    REQUIRE(direct.route.has_value());
    const simple_platformer::Route directRoute = routeOf(direct);
    REQUIRE(directRoute.steps.size() == 1);
    REQUIRE(directRoute.steps.front().traversal == simple_platformer::Traversal::Jump);
    REQUIRE(directRoute.steps.front().inputs.size() == 1);
}

TEST_CASE(
    "A search rejects cells off its grid, missing functions and costs below one",
    "[navigation][search][validation]")
{
    const auto leadsOut = [](RouteLocation)
    {
        return std::vector<simple_platformer::RouteConnection>{
            {{{{8, 0}}, simple_platformer::Traversal::Fly, {}}, 1}};
    };
    REQUIRE_THROWS_AS(
        simple_platformer::findLowestCostRoute(
            floorOf(0, 0), floorOf(1, 0).cell, TestGrid, leadsOut, zeroHeuristic),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findLowestCostRoute(
            floorOf(-1, 0), floorOf(1, 0).cell, TestGrid, noConnections, zeroHeuristic),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findLowestCostRoute(
            floorOf(0, 0), floorOf(1, 0).cell, {0, 8}, noConnections, zeroHeuristic),
        std::invalid_argument);

    const simple_platformer::ConnectionFunction missingConnections;
    const simple_platformer::HeuristicFunction missingHeuristic;
    const simple_platformer::HeuristicFunction negativeHeuristic = [](Cell, Cell) { return -1; };
    REQUIRE_THROWS_AS(
        simple_platformer::findLowestCostRoute(
            floorOf(0, 0), floorOf(1, 0).cell, TestGrid, missingConnections, zeroHeuristic),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findLowestCostRoute(
            floorOf(0, 0), {1, 0}, TestGrid, noConnections, missingHeuristic),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findLowestCostRoute(
            floorOf(0, 0), floorOf(1, 0).cell, TestGrid, noConnections, negativeHeuristic),
        std::invalid_argument);

    const auto costsNothing = [](RouteLocation)
    {
        return std::vector<simple_platformer::RouteConnection>{
            {{{{1, 0}}, simple_platformer::Traversal::Walk, {}}, 0}};
    };
    REQUIRE_THROWS_AS(
        simple_platformer::findLowestCostRoute(
            floorOf(0, 0), floorOf(1, 0).cell, TestGrid, costsNothing, zeroHeuristic),
        std::invalid_argument);
}

TEST_CASE(
    "A pending location pauses the search before its connections are requested",
    "[navigation][search]")
{
    int connectionQueries = 0;
    const simple_platformer::ConnectionFunction connections = [&](RouteLocation location)
    {
        ++connectionQueries;
        if (location.cell == Cell{0, 0})
        {
            return std::vector<simple_platformer::RouteConnection>{
                {{{{1, 0}}, simple_platformer::Traversal::Walk, {}}, 1}};
        }
        return std::vector<simple_platformer::RouteConnection>{};
    };
    const simple_platformer::ExpansionReady canExpand = [](RouteLocation location)
    { return location.cell != Cell{1, 0}; };

    const RouteSearchResult result = simple_platformer::findLowestCostRoute(
        floorOf(0, 0), floorOf(2, 0).cell, TestGrid, connections, gridSteps, canExpand);
    REQUIRE(result.unexpandedLocation.has_value());
    REQUIRE_FALSE(result.route.has_value());
    REQUIRE(result.unexpandedLocation == floorOf(1, 0));
    REQUIRE(connectionQueries == 1);
}

TEST_CASE("A search stops at the cheapest location in the goal cell", "[navigation][search]")
{
    using simple_platformer::ClimbSurface;
    using simple_platformer::RouteConnection;
    using simple_platformer::Traversal;

    // Both the floor and the wall of the goal cell arrive; the cheaper is chosen.
    const simple_platformer::ConnectionFunction connections = connectionsFrom(
        {{{0, 0},
          {{{{{1, 0}}, Traversal::Walk, {}}, 5},
           {{{{1, 0}, ClimbSurface::LeftWall}, Traversal::Climb, {}}, 2}}}});

    const RouteSearchResult result = simple_platformer::findLowestCostRoute(
        floorOf(0, 0), {1, 0}, TestGrid, connections, zeroHeuristic);
    REQUIRE(result.route.has_value());
    REQUIRE(
        simple_platformer::endOf(routeOf(result)) == RouteLocation{{1, 0}, ClimbSurface::LeftWall});
}

TEST_CASE("A goal off the grid is searched for and never reached", "[navigation][search]")
{
    const auto forwardOnly = [](RouteLocation location)
    {
        const Cell position = location.cell;
        if (position.x < 7)
        {
            return std::vector<simple_platformer::RouteConnection>{
                {{{{position.x + 1, position.y}}, simple_platformer::Traversal::Fly, {}}, 1}};
        }
        return std::vector<simple_platformer::RouteConnection>{};
    };
    const RouteSearchResult result = simple_platformer::findLowestCostRoute(
        floorOf(0, 0), {20, 0}, TestGrid, forwardOnly, gridSteps);
    REQUIRE(result.route.has_value());
    REQUIRE(simple_platformer::endOf(routeOf(result)) == floorOf(7, 0));
}
