#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

namespace simple_platformer
{
    // Hands back the connections leaving a cell, built on the spot.
    using GridNeighborFunction = std::function<std::vector<NavigationNeighbor>(GridPosition cell)>;
    // Given each connection leaving a cell, with the cost the search charges for it: the
    // connection's own unless the policy adds to it, as a platformer search does to a jump.
    using GridNeighborVisitor = std::function<void(const NavigationNeighbor& neighbor, int cost)>;
    // Visits the connections leaving a cell where they are, so a policy that keeps them,
    // such as one reading a cache, need not copy them out for every search.
    using GridNeighborVisitFunction =
        std::function<void(GridPosition cell, const GridNeighborVisitor& visit)>;
    using GridHeuristicFunction = std::function<int(GridPosition cell, GridPosition goal)>;

    // What a search cost, for the frame profile.
    struct PathSearchStatistics
    {
        // Cells whose connections the search asked for.
        int nodesExpanded = 0;
        // Of those, cells whose connections a cache already held.
        int cellsReused = 0;
        // Movement ticks simulated to build connections; only platformer searches do this.
        int simulatedTicks = 0;
    };

    int manhattanHeuristic(GridPosition cell, GridPosition goal);

    // Uses no heuristic and always searches for the lowest accumulated connection cost.
    std::optional<NavigationPath> findLowestCostPath(
        GridPosition start,
        GridPosition goal,
        const GridNeighborFunction& neighbors);

    // Uses A* ordering. The heuristic must never overestimate the remaining cost. With
    // statistics, counts the cells it expanded. When there is no path and reached is
    // given, fills it with every cell the search got to, which is every cell reachable
    // from the start; a search that finds a path leaves it alone.
    std::optional<NavigationPath> findLowestCostPath(
        GridPosition start,
        GridPosition goal,
        const GridNeighborFunction& neighbors,
        const GridHeuristicFunction& heuristic,
        PathSearchStatistics* statistics = nullptr,
        std::vector<GridPosition>* reached = nullptr);

    // The same search over connections visited in place. Only the connections the search
    // follows are copied, into the path.
    std::optional<NavigationPath> findLowestCostPath(
        GridPosition start,
        GridPosition goal,
        const GridNeighborVisitFunction& visitNeighbors,
        const GridHeuristicFunction& heuristic,
        PathSearchStatistics* statistics = nullptr,
        std::vector<GridPosition>* reached = nullptr);
}
