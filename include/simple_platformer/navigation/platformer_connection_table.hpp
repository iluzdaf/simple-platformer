#pragma once

#include <cstddef>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/route.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"

namespace simple_platformer
{
    class TileMap;

    // Stores every cell's connections for one map, grouped by traversal profile.
    // Preparing a new profile builds the whole map. A tile break rebuilds only cells
    // whose simulation footprints include it.
    class PlatformerConnectionTable
    {
    public:
        // Applies new breaks to existing profiles, then builds this profile if needed.
        // Body size and simulation step must be finite and positive.
        void prepare(const TileMap& map, const PlatformerTraversalProfile& profile);

        // For every built profile, rebuilds cells whose footprints include a new break.
        // The new connections and footprints use the map's current tiles.
        void applyRecordedTileBreaks(const TileMap& map);

        bool isBuilt(const PlatformerTraversalProfile& profile) const;

        // The connections leaving every surface of the cell, in a built profile. Throws when
        // the profile is not built or the cell is off the map.
        const std::vector<RouteConnection>& connections(
            Cell cell,
            const PlatformerTraversalProfile& profile) const;
        // Tiles read or swept while simulating this cell, including a margin for support
        // checks. A break inside this range rebuilds the connections. Throws if the profile
        // is not built or the cell is off the map.
        const CellRange& footprint(Cell cell, const PlatformerTraversalProfile& profile) const;

    private:
        struct CellConnections
        {
            std::vector<RouteConnection> connections;
            CellRange footprint;
        };

        struct ProfileTable
        {
            PlatformerTraversalProfile profile;
            GridSize grid;
            // Row-major storage: the cell at (x, y) is at y * grid.width + x.
            std::vector<CellConnections> cells;
        };

        const ProfileTable* findTable(const PlatformerTraversalProfile& profile) const;
        const CellConnections& entry(Cell cell, const PlatformerTraversalProfile& profile) const;

        std::vector<ProfileTable> profileTables;
        // How far into the map's log of broken cells the table has applied.
        std::size_t breaksSeen = 0;
    };
}
