#pragma once

#include <cstddef>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/route.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"

namespace simple_platformer
{
    class TileMap;

    // Every cell's platformer connections for one map, kept per traversal profile. A
    // profile is built whole, for every cell, the first time it is prepared; a level
    // prepares its NPCs' profiles when it starts. A tile broken after that rebuilds only
    // the cells whose footprint holds it.
    class PlatformerConnectionTable
    {
    public:
        // Applies the map's breaks to the built profiles, then builds this profile for
        // every cell if it is not built yet. The profile needs a finite, positive size and
        // step.
        void prepare(const TileMap& map, const PlatformerTraversalProfile& profile);

        // Rebuilds, in every built profile, the cells whose footprint holds a tile broken
        // since the last call. Their new footprints come from the map as it is now.
        void applyRecordedTileBreaks(const TileMap& map);

        bool isBuilt(const PlatformerTraversalProfile& profile) const;

        // The connections leaving every surface of the cell, in a built profile. Throws when
        // the profile is not built or the cell is off the map.
        const std::vector<RouteConnection>& connections(
            Cell cell,
            const PlatformerTraversalProfile& profile) const;
        // The cells the cell's connections were simulated over. A break inside it rebuilds
        // them. Throws as connections does.
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
            // Row by row, a cell at y * grid.width + x.
            std::vector<CellConnections> cells;
        };

        const ProfileTable* findTable(const PlatformerTraversalProfile& profile) const;
        const CellConnections& entry(Cell cell, const PlatformerTraversalProfile& profile) const;

        std::vector<ProfileTable> profileTables;
        // How far into the map's log of broken cells the table has applied.
        std::size_t breaksSeen = 0;
    };
}
