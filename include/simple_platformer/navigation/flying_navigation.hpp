#pragma once

#include <vector>

#include "simple_platformer/math/coordinates.hpp"

namespace simple_platformer
{
    class TileMap;

    std::vector<GridPosition> flyingNeighbors(const TileMap& map, GridPosition position);
}
