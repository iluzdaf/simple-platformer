#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

namespace simple_platformer
{
    // Candidate destination cells the search may consider from a cell. A candidate
    // needs a connection before the search can reach it.
    using GridNeighborFunction = std::function<std::vector<GridPosition>(GridPosition cell)>;
    // A policy returns the connections leaving a cell. Each connection supplies its cost.
    using GridConnectionFunction =
        std::function<std::vector<NavigationConnection>(GridPosition cell)>;
    // A non-negative lower bound on the cost from a cell to the goal.
    using GridHeuristicFunction = std::function<int(GridPosition cell, GridPosition goal)>;
    // False pauses expansion of a non-goal cell before its neighbors or connections are
    // requested. An empty function allows every cell; a cache may wait for pending work.
    using GridExpansionReady = std::function<bool(GridPosition cell)>;

    enum class PathSearchStatus
    {
        Found,
        Unreachable,
        Incomplete
    };

    // Found carries a path, Unreachable carries all cells reachable from the start,
    // and Incomplete identifies the cell whose expansion was denied.
    struct PathSearchResult
    {
        PathSearchStatus status = PathSearchStatus::Unreachable;
        std::optional<NavigationPath> path;
        std::vector<GridPosition> reachableCells;
        std::optional<GridPosition> unexpandedCell;
    };

    int manhattanHeuristic(GridPosition cell, GridPosition goal);

    // A* over connections to candidate neighbors. The heuristic must not overestimate;
    // zero gives Dijkstra's search. Start, goal, candidates, and connection destinations
    // must lie within the grid. If canExpand rejects a cell, the result is Incomplete.
    PathSearchResult findLowestCostPath(
        GridPosition start,
        GridPosition goal,
        GridSize grid,
        const GridNeighborFunction& neighbors,
        const GridConnectionFunction& connections,
        const GridHeuristicFunction& heuristic,
        const GridExpansionReady& canExpand = {});
}
