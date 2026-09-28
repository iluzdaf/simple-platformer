#pragma once

#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_search.hpp"

namespace simple_platformer
{
    class TileMap;

    // Adjacent cells that permit flying movement. This policy identifies cells, not
    // the traversal and cost of the connections passed to path search.
    std::vector<GridPosition> flyingNeighbors(const TileMap& map, GridPosition cell);

    // Cost-one flight connections to the cells selected by flyingNeighbors.
    std::vector<NavigationConnection> flyingConnections(const TileMap& map, GridPosition cell);

    // The cheapest flight from one cell to another: every cell that allows movement is a
    // node, joined to its four neighbors at a cost of one. No path when either cell is
    // off the map. Optional statistics count expanded cells.
    NavigationPathResult findFlyingPath(
        const TileMap& map,
        GridPosition start,
        GridPosition goal,
        PathSearchStatistics* statistics = nullptr);
}
