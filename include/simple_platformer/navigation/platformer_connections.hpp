#pragma once

#include <optional>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_graph.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"

namespace simple_platformer
{
    class PlatformerConnectionCache;
    class TileMap;

    // What a walk of some number of cells along a floor cost when it was simulated, and
    // the cells its simulation swept, as offsets from the cell it started in. No cost
    // when the body could not reach the cell and stop within the simulation limit.
    // Simulated ticks describe the original walk, not work done when it is reused.
    struct WalkSimulationResult
    {
        // Signed distance from the start cell; also the walk cache's key.
        int columns = 0;
        std::optional<int> cost;
        CellRange sweep;
        int simulatedTicks = 0;
    };

    struct BuiltPlatformerConnections
    {
        std::vector<NavigationConnection> connections;
        // Conservative rectangle covering the tiles probed or swept by simulation.
        CellRange footprint;
        std::vector<WalkSimulationResult> walksToCache;
        // Movement ticks simulated for this build; reused walks add none.
        int simulatedTicks = 0;
    };

    // Builds connections without changing the cache. A read-only cache can reuse
    // previously simulated walks; newly simulated walks are returned for later storage.
    BuiltPlatformerConnections buildPlatformerConnections(
        const TileMap& map,
        Cell cell,
        const PlatformerTraversalProfile& profile,
        const PlatformerConnectionCache* walkCache = nullptr);

    // Stores a completed build; it does not simulate connections or check for a hit.
    void storePlatformerConnections(
        PlatformerConnectionCache& cache,
        Cell cell,
        const PlatformerTraversalProfile& profile,
        BuiltPlatformerConnections built);

}
