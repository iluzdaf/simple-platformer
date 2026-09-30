#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/route.hpp"

namespace simple_platformer
{
    // Returns the connections leaving a location. Each is one step to a place nearby,
    // with its cost and the inputs that make the step.
    using ConnectionFunction = std::function<std::vector<RouteConnection>(RouteLocation location)>;
    // Guesses the cost from a cell to the goal cell. The guess must never be more than
    // the real cost, and never below zero.
    using HeuristicFunction = std::function<int(Cell cell, Cell goal)>;
    // Says whether the search may expand a location now. False pauses the search there,
    // before it asks for that location's connections, so a cache can finish its work
    // first. An empty function allows every location.
    using ExpansionReady = std::function<bool(RouteLocation location)>;

    // What a search ends with. A finished search has a route to the cheapest location in
    // the goal cell. If it could not reach that cell, the route leads to a cell as close
    // to it as possible, the first reached of any equally close. A paused search has no
    // route, and names the location it stopped at.
    struct RouteSearchResult
    {
        std::optional<Route> route;
        std::optional<RouteLocation> unexpandedLocation;
    };

    // Finds the cheapest route from start to any location in the goal cell, using A*.
    // A location is a cell and a surface, so a cell's floor, walls and ceiling are
    // separate places. A search without climbing only uses floors. The start and every
    // connection's destination must be on the grid, but the goal need not be. A
    // heuristic that always guesses zero turns A* into Dijkstra's search.
    RouteSearchResult findLowestCostRoute(
        RouteLocation start,
        Cell goal,
        GridSize grid,
        const ConnectionFunction& connections,
        const HeuristicFunction& heuristic,
        const ExpansionReady& canExpand = {});
}
