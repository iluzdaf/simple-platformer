#pragma once

#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/route.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"

namespace simple_platformer
{
    class TileMap;

    struct BuiltPlatformerConnections
    {
        std::vector<RouteConnection> connections;
        // Conservative rectangle covering the tiles probed or swept by simulation.
        CellRange footprint;
        // Movement ticks simulated for this build.
        int simulatedTicks = 0;
    };

    // Simulates connections against the current map.
    BuiltPlatformerConnections buildPlatformerConnections(
        const TileMap& map,
        Cell cell,
        const PlatformerTraversalProfile& profile);

}
