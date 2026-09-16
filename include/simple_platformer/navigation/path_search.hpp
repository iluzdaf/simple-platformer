#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

namespace simple_platformer
{
    using GridNeighborFunction =
        std::function<std::vector<NavigationNeighbor>(GridPosition position)>;
    using GridHeuristicFunction = std::function<int(GridPosition position, GridPosition goal)>;

    int manhattanHeuristic(GridPosition position, GridPosition goal);

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
