#pragma once

#include <optional>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

namespace simple_platformer
{
    class TileMap;

    // High-level path API for flying actors.
    std::optional<NavigationPath> findFlyingPath(
        const TileMap& map,
        GridPosition start,
        GridPosition goal);

    // Lower-level policy used by the generic path search.
    std::vector<NavigationNeighbor> flyingNeighbors(const TileMap& map, GridPosition position);
}
