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

    int manhattanHeuristic(GridPosition cell, GridPosition goal);

    // Uses no heuristic and always searches for the lowest accumulated connection cost.
    std::optional<NavigationPath> findLowestCostPath(
        GridPosition start,
        GridPosition goal,
        const GridNeighborFunction& neighbors);

    // Uses A* ordering. The heuristic must never overestimate the remaining cost.
    std::optional<NavigationPath> findLowestCostPath(
        GridPosition start,
        GridPosition goal,
        const GridNeighborFunction& neighbors,
        const GridHeuristicFunction& heuristic);
}
