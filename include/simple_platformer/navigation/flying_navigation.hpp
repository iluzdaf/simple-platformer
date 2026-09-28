#pragma once

#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

namespace simple_platformer
{
    class TileMap;
    struct FrameProfile;

    // Cost-one flight connections to adjacent open cells.
    std::vector<NavigationConnection> flyingConnections(const TileMap& map, GridPosition cell);

    // The cheapest flight from one cell to another: every cell that allows movement is a
    // node, joined to its four neighbors at a cost of one. No path when either cell is
    // off the map. An optional frame profile records search work.
    NavigationPathResult findFlyingPath(
        const TileMap& map,
        GridPosition start,
        GridPosition goal,
        FrameProfile* profile = nullptr);
}
