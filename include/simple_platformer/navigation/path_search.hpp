#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

namespace simple_platformer
{
    using GridNeighborFunction = std::function<std::vector<NavigationNeighbor>(GridPosition cell)>;
    using GridHeuristicFunction = std::function<int(GridPosition cell, GridPosition goal)>;

    // What a search cost, for the frame profile.
    struct PathSearchStatistics
    {
        // Cells whose connections the search asked for.
        int nodesExpanded = 0;
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
    // statistics, counts the cells it expanded.
    std::optional<NavigationPath> findLowestCostPath(
        GridPosition start,
        GridPosition goal,
        const GridNeighborFunction& neighbors,
        const GridHeuristicFunction& heuristic,
        PathSearchStatistics* statistics = nullptr);
}
