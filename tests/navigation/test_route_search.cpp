#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "simple_platformer/input/input_program.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/navigation/route.hpp"
#include "simple_platformer/navigation/route_search.hpp"
#include "simple_platformer/navigation/traversal.hpp"

namespace
{
    using simple_platformer::Cell;
    using simple_platformer::ClimbSurface;
    using simple_platformer::ConnectionFunction;
    using simple_platformer::endOf;
    using simple_platformer::findLowestCostRoute;
    using simple_platformer::InputProgram;
    using simple_platformer::Route;
    using simple_platformer::RouteConnection;
    using simple_platformer::RouteLocation;
    using simple_platformer::RouteSearchResult;
    using simple_platformer::Traversal;

    constexpr simple_platformer::GridSize TestGrid{8, 8};

    // The floor of the cell, the only location a search without climbing uses.
    RouteLocation floorOf(int x, int y)
    {
        return {{x, y}};
    }

    RouteConnection connectionTo(
        RouteLocation destination,
        Traversal traversal,
        int cost,
        InputProgram inputs = {})
    {
        return {{destination, traversal, std::move(inputs)}, cost};
    }

    // The same connections leave every location of a cell listed; any other cell has none.
    ConnectionFunction connectionsFrom(
        std::vector<std::pair<Cell, std::vector<RouteConnection>>> table)
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
            return std::vector<RouteConnection>{};
        };
    }

    // A row of cells, each leading to the next for a cost of 1, up to the last column.
    ConnectionFunction lineUpTo(int lastColumn)
    {
        return [lastColumn](RouteLocation location)
        {
            const Cell cell = location.cell;
            if (cell.x >= lastColumn)
            {
                return std::vector<RouteConnection>{};
            }
            return std::vector<RouteConnection>{
                connectionTo(floorOf(cell.x + 1, cell.y), Traversal::Fly, 1)};
        };
    }

    std::vector<RouteConnection> noConnections(RouteLocation)
    {
        return {};
    }

    int zeroHeuristic(Cell, Cell)
    {
        return 0;
    }

    // The number of cells between two cells along the grid. It never guesses more than
    // the real cost when every step moves one cell for a cost of 1.
    int gridSteps(Cell cell, Cell goal)
    {
        return std::abs(cell.x - goal.x) + std::abs(cell.y - goal.y);
    }

    Route routeOf(const RouteSearchResult& result)
    {
        return result.route.value_or(Route{});
    }
}

TEST_CASE(
    "A search that cannot move or starts in the goal cell returns a route with no steps",
    "[navigation][search]")
{
    const RouteSearchResult unreachable =
        findLowestCostRoute(floorOf(0, 0), {1, 0}, TestGrid, noConnections, zeroHeuristic);
    REQUIRE(unreachable.route.has_value());
    REQUIRE(routeOf(unreachable).steps.empty());

    const RouteSearchResult alreadyThere =
        findLowestCostRoute(floorOf(2, 3), {2, 3}, TestGrid, noConnections, zeroHeuristic);
    REQUIRE(alreadyThere.route.has_value());
    const Route route = routeOf(alreadyThere);
    REQUIRE(route.start.cell == Cell{2, 3});
    REQUIRE(route.steps.empty());
}

TEST_CASE(
    "A search treats a cell's floor, walls and ceiling as separate places",
    "[navigation][search]")
{
    // In the first cell, only the ceiling leads on to the goal cell. Going from the floor
    // to the ceiling by way of the wall is cheaper than climbing straight up, so the
    // route should go floor, wall, ceiling. It can only do that if the search keeps the
    // three apart.
    const RouteLocation wall{{0, 0}, ClimbSurface::LeftWall};
    const RouteLocation ceiling{{0, 0}, ClimbSurface::Ceiling};
    const ConnectionFunction connections = [&](RouteLocation location)
    {
        switch (location.surface)
        {
        case ClimbSurface::None:
            return std::vector<RouteConnection>{
                connectionTo(ceiling, Traversal::Climb, 5),
                connectionTo(wall, Traversal::Climb, 1)};
        case ClimbSurface::LeftWall:
            return std::vector<RouteConnection>{connectionTo(ceiling, Traversal::Climb, 1)};
        case ClimbSurface::Ceiling:
            return std::vector<RouteConnection>{
                connectionTo({{1, 0}, ClimbSurface::Ceiling}, Traversal::Climb, 1)};
        case ClimbSurface::RightWall:
            break;
        }
        return std::vector<RouteConnection>{};
    };

    const RouteSearchResult result =
        findLowestCostRoute(floorOf(0, 0), {1, 0}, {2, 1}, connections, zeroHeuristic);
    REQUIRE(result.route.has_value());
    const Route route = routeOf(result);
    REQUIRE(route.steps.size() == 3);
    REQUIRE(route.steps[0].destination == wall);
    REQUIRE(route.steps[1].destination == ceiling);
    REQUIRE(route.steps[2].destination.cell == Cell{1, 0});
}

TEST_CASE(
    "A search picks the cheapest route, however many cells each connection crosses",
    "[navigation][search]")
{
    // A jump straight to the goal costs more than two walks by way of the middle cell,
    // so the route takes the walks.
    const ConnectionFunction connections = connectionsFrom(
        {{{0, 0},
          {connectionTo(floorOf(2, 0), Traversal::Jump, 8),
           connectionTo(floorOf(1, 0), Traversal::Walk, 1)}},
         {{1, 0}, {connectionTo(floorOf(2, 0), Traversal::Walk, 1)}}});
    const RouteSearchResult walks =
        findLowestCostRoute(floorOf(0, 0), {2, 0}, TestGrid, connections, zeroHeuristic);
    REQUIRE(walks.route.has_value());
    const Route walkRoute = routeOf(walks);
    REQUIRE(walkRoute.steps.size() == 2);
    REQUIRE(walkRoute.steps.front().traversal == Traversal::Walk);

    // One connection can cross several cells and still cost less than the number of
    // cells it crosses.
    const ConnectionFunction leaping =
        connectionsFrom({{{0, 0}, {connectionTo(floorOf(4, 0), Traversal::Jump, 2)}}});
    const RouteSearchResult leap =
        findLowestCostRoute(floorOf(0, 0), {4, 0}, TestGrid, leaping, zeroHeuristic);
    REQUIRE(leap.route.has_value());
    const Route leapRoute = routeOf(leap);
    REQUIRE(leapRoute.steps.size() == 1);
    REQUIRE(leapRoute.steps.front().destination == floorOf(4, 0));
}

TEST_CASE(
    "A search that cannot reach the goal cell returns a route as close to it as possible",
    "[navigation][search]")
{
    // A line of three cells. Nothing leads beyond the last.
    const ConnectionFunction line = lineUpTo(2);

    // The goal is past the end of the line, so the route stops at the last cell.
    const RouteSearchResult outOfReach =
        findLowestCostRoute(floorOf(0, 0), {5, 0}, TestGrid, line, gridSteps);
    REQUIRE(outOfReach.route.has_value());
    REQUIRE(endOf(routeOf(outOfReach)) == floorOf(2, 0));
    REQUIRE(routeOf(outOfReach).steps.size() == 2);

    // The same last cell as the goal is reached, and ends the route the same way.
    const RouteSearchResult inReach =
        findLowestCostRoute(floorOf(0, 0), {2, 0}, TestGrid, line, gridSteps);
    REQUIRE(inReach.route.has_value());
    REQUIRE(endOf(routeOf(inReach)) == floorOf(2, 0));
}

TEST_CASE(
    "A search chooses by the connections' costs, not by how many steps a route takes",
    "[navigation][search]")
{
    // Two ways from the start to the goal: a jump straight there, and two walks by way of
    // a middle cell. Changing the jump's cost changes which route is chosen.
    const auto withJumpCosting = [](int jumpCost)
    {
        return connectionsFrom(
            {{{0, 0},
              {connectionTo(floorOf(2, 0), Traversal::Jump, jumpCost, {{0.5F, {}}}),
               connectionTo(floorOf(1, 0), Traversal::Walk, 1)}},
             {{1, 0}, {connectionTo(floorOf(2, 0), Traversal::Walk, 1)}}});
    };

    // With the jump costing 6, the two walks at 1 each are cheaper.
    const RouteSearchResult aroundJump =
        findLowestCostRoute(floorOf(0, 0), {2, 0}, TestGrid, withJumpCosting(6), zeroHeuristic);
    REQUIRE(aroundJump.route.has_value());
    const Route walkRoute = routeOf(aroundJump);
    REQUIRE(walkRoute.steps.size() == 2);
    REQUIRE(walkRoute.steps.front().traversal == Traversal::Walk);

    // With the jump costing 1, it is cheaper, and the route keeps its inputs.
    const RouteSearchResult overJump =
        findLowestCostRoute(floorOf(0, 0), {2, 0}, TestGrid, withJumpCosting(1), zeroHeuristic);
    REQUIRE(overJump.route.has_value());
    const Route jumpRoute = routeOf(overJump);
    REQUIRE(jumpRoute.steps.size() == 1);
    REQUIRE(jumpRoute.steps.front().traversal == Traversal::Jump);
    REQUIRE(jumpRoute.steps.front().inputs.size() == 1);
}

TEST_CASE(
    "A search rejects places off its grid, missing functions and costs below one",
    "[navigation][search][validation]")
{
    // A connection leading off the grid, a start off the grid, and a grid with no cells.
    const ConnectionFunction leadsOut =
        connectionsFrom({{{0, 0}, {connectionTo(floorOf(8, 0), Traversal::Fly, 1)}}});
    REQUIRE_THROWS_AS(
        findLowestCostRoute(floorOf(0, 0), {1, 0}, TestGrid, leadsOut, zeroHeuristic),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        findLowestCostRoute(floorOf(-1, 0), {1, 0}, TestGrid, noConnections, zeroHeuristic),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        findLowestCostRoute(floorOf(0, 0), {1, 0}, {0, 8}, noConnections, zeroHeuristic),
        std::invalid_argument);

    // A missing connection function, a missing heuristic, and a heuristic that guesses
    // below zero.
    const ConnectionFunction missingConnections;
    const simple_platformer::HeuristicFunction missingHeuristic;
    const simple_platformer::HeuristicFunction negativeHeuristic = [](Cell, Cell) { return -1; };
    REQUIRE_THROWS_AS(
        findLowestCostRoute(floorOf(0, 0), {1, 0}, TestGrid, missingConnections, zeroHeuristic),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        findLowestCostRoute(floorOf(0, 0), {1, 0}, TestGrid, noConnections, missingHeuristic),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        findLowestCostRoute(floorOf(0, 0), {1, 0}, TestGrid, noConnections, negativeHeuristic),
        std::invalid_argument);

    // A connection that costs nothing.
    const ConnectionFunction costsNothing =
        connectionsFrom({{{0, 0}, {connectionTo(floorOf(1, 0), Traversal::Walk, 0)}}});
    REQUIRE_THROWS_AS(
        findLowestCostRoute(floorOf(0, 0), {1, 0}, TestGrid, costsNothing, zeroHeuristic),
        std::invalid_argument);
}

TEST_CASE(
    "A search pauses at a location it may not expand yet, before asking for its connections",
    "[navigation][search]")
{
    // The start leads to (1, 0), which the readiness check refuses. The search expands
    // the start, reaches (1, 0) and pauses there: no route, only the location, and it
    // never asks for (1, 0)'s connections.
    const ConnectionFunction line = lineUpTo(2);
    int connectionQueries = 0;
    const ConnectionFunction countedLine = [&](RouteLocation location)
    {
        ++connectionQueries;
        return line(location);
    };
    const simple_platformer::ExpansionReady canExpand = [](RouteLocation location)
    { return location.cell != Cell{1, 0}; };

    const RouteSearchResult paused =
        findLowestCostRoute(floorOf(0, 0), {2, 0}, TestGrid, countedLine, gridSteps, canExpand);
    REQUIRE_FALSE(paused.route.has_value());
    REQUIRE(paused.unexpandedLocation == floorOf(1, 0));
    REQUIRE(connectionQueries == 1);
}

TEST_CASE("A search stops at the cheapest location in the goal cell", "[navigation][search]")
{
    // The goal cell can be reached on its floor or on its wall. The wall costs less, so
    // the route ends there.
    const RouteLocation goalWall{{1, 0}, ClimbSurface::LeftWall};
    const ConnectionFunction connections = connectionsFrom(
        {{{0, 0},
          {connectionTo(floorOf(1, 0), Traversal::Walk, 5),
           connectionTo(goalWall, Traversal::Climb, 2)}}});

    const RouteSearchResult result =
        findLowestCostRoute(floorOf(0, 0), {1, 0}, TestGrid, connections, zeroHeuristic);
    REQUIRE(result.route.has_value());
    REQUIRE(endOf(routeOf(result)) == goalWall);
}

TEST_CASE(
    "A goal off the grid is never reached, so the route gets as close as it can",
    "[navigation][search]")
{
    // A line along the top row to the grid's last column. The goal is past the grid's
    // edge, so the route stops at the end of the line.
    const RouteSearchResult offGrid =
        findLowestCostRoute(floorOf(0, 0), {20, 0}, TestGrid, lineUpTo(7), gridSteps);
    REQUIRE(offGrid.route.has_value());
    REQUIRE(endOf(routeOf(offGrid)) == floorOf(7, 0));
}
