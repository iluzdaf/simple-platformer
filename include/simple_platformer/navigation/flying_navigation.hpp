#pragma once

#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

namespace simple_platformer
{
    class TileMap;

    std::vector<NavigationNeighbor> flyingNeighbors(const TileMap& map, GridPosition position);
}
