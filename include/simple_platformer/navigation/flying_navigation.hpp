#pragma once

#include <optional>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_search.hpp"

namespace simple_platformer
{
    class TileMap;

    // The cheapest flight from one cell to another: every cell that allows movement is a
    // node, joined to its four neighbours at a cost of one. No path when either cell is
    // off the map. With statistics, reports what the search cost.
    std::optional<NavigationPath> findFlyingPath(
        const TileMap& map,
        GridPosition start,
        GridPosition goal,
        PathSearchStatistics* statistics = nullptr);

    // The neighbouring cells that allow movement, which is what the flying search visits.
    std::vector<NavigationNeighbor> flyingNeighbors(const TileMap& map, GridPosition cell);
}
