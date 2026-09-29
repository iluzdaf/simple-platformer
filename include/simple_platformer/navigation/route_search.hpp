#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/route.hpp"

namespace simple_platformer
{
    // A policy returns the connections leaving a location. Each destination is a
    // reachable neighbor; the connection supplies its cost and traversal inputs.
    using ConnectionFunction = std::function<std::vector<RouteConnection>(RouteLocation location)>;
    // A non-negative lower bound on the cost from a cell to the goal cell.
    using HeuristicFunction = std::function<int(Cell cell, Cell goal)>;
    // False pauses expansion of a non-goal location before its connections are
    // requested. An empty function allows every location; a cache may wait for
    // pending work.
    using ExpansionReady = std::function<bool(RouteLocation location)>;

    // A finished search has a route: to the cheapest location in the goal cell, or when
    // it cannot reach that cell, to the reached location whose cell is nearest it. A
    // paused one has no route and names the location whose expansion was refused.
    struct RouteSearchResult
    {
        std::optional<Route> route;
        std::optional<RouteLocation> unexpandedLocation;
    };

    // A* over outgoing connections to any location in the goal cell. The heuristic
    // must not overestimate; zero gives Dijkstra's search. A location is a cell and a
    // surface, so the floor, walls, and ceiling of one cell are distinct nodes; a
    // policy without climbing uses only the floor. The start and connection
    // destinations must lie within the grid; the goal need not. Of equally near
    // cells, a failed search keeps the first it reached. If canExpand rejects a
    // location, the search pauses there.
    RouteSearchResult findLowestCostRoute(
        RouteLocation start,
        Cell goal,
        GridSize grid,
        const ConnectionFunction& connections,
        const HeuristicFunction& heuristic,
        const ExpansionReady& canExpand = {});
}
