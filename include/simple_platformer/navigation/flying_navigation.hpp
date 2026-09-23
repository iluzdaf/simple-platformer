#pragma once

#include <optional>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_search.hpp"

namespace simple_platformer
{
    class TileMap;

    // High-level path API for flying actors. With statistics, reports what the search cost.
    std::optional<NavigationPath> findFlyingPath(
        const TileMap& map,
        GridPosition start,
        GridPosition goal,
        PathSearchStatistics* statistics = nullptr);

    // Lower-level policy used by the generic path search.
    std::vector<NavigationNeighbor> flyingNeighbors(const TileMap& map, GridPosition cell);
}
