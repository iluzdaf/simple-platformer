#include "simple_platformer/navigation/platformer_connection_table.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/navigation/route.hpp"
#include "simple_platformer/navigation/platformer_connections.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/timing/frame_profile.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    namespace
    {
        std::size_t indexOf(GridSize grid, Cell cell)
        {
            return static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(grid.width) +
                   static_cast<std::size_t>(cell.x);
        }
    }

    void PlatformerConnectionTable::prepare(
        const TileMap& map,
        const PlatformerTraversalProfile& profile,
        FrameProfile* frameProfile)
    {
        if (!isFinitePositive(profile.size) || !isFinitePositive(profile.stepSeconds))
        {
            throw std::invalid_argument(
                "Connections require a finite, positive profile size and step");
        }
        const GridSize grid = map.size();
        if (!profileTables.empty() && (profileTables.front().grid.width != grid.width ||
                                       profileTables.front().grid.height != grid.height))
        {
            throw std::logic_error("A connection table belongs to one map");
        }
        applyRecordedTileBreaks(map, frameProfile);
        if (isBuilt(profile))
        {
            return;
        }

        ProfileTable table{profile, grid, {}};
        table.cells.reserve(static_cast<std::size_t>(grid.width) * grid.height);
        int simulatedTicks = 0;
        for (int row = 0; row < grid.height; ++row)
        {
            for (int column = 0; column < grid.width; ++column)
            {
                BuiltPlatformerConnections built =
                    buildPlatformerConnections(map, {column, row}, profile);
                simulatedTicks += built.simulatedTicks;
                table.cells.push_back({std::move(built.connections), built.footprint});
            }
        }
        profileTables.push_back(std::move(table));
        addFrameStatistic(
            frameProfile,
            "Navigation",
            "Cells built",
            static_cast<int>(profileTables.back().cells.size()));
        addFrameStatistic(frameProfile, "Navigation", "Build simulated ticks", simulatedTicks);
    }

    void PlatformerConnectionTable::applyRecordedTileBreaks(
        const TileMap& map,
        FrameProfile* frameProfile)
    {
        const std::vector<Cell>& broken = map.brokenCells();
        if (breaksSeen > broken.size())
        {
            throw std::logic_error("A connection table belongs to one map");
        }
        if (breaksSeen == broken.size())
        {
            return;
        }
        const auto unseen = broken.begin() + static_cast<std::ptrdiff_t>(breaksSeen);
        int cellsRebuilt = 0;
        int simulatedTicks = 0;
        for (ProfileTable& table : profileTables)
        {
            for (int row = 0; row < table.grid.height; ++row)
            {
                for (int column = 0; column < table.grid.width; ++column)
                {
                    const Cell cell{column, row};
                    CellConnections& stored = table.cells[indexOf(table.grid, cell)];
                    const bool touched = std::any_of(
                        unseen,
                        broken.end(),
                        [&stored](Cell brokenCell)
                        { return contains(stored.footprint, brokenCell); });
                    if (!touched)
                    {
                        continue;
                    }
                    BuiltPlatformerConnections built =
                        buildPlatformerConnections(map, cell, table.profile);
                    simulatedTicks += built.simulatedTicks;
                    stored = {std::move(built.connections), built.footprint};
                    ++cellsRebuilt;
                }
            }
        }
        addFrameStatistic(
            frameProfile,
            "Navigation",
            "Tile breaks applied",
            static_cast<int>(broken.size() - breaksSeen));
        addFrameStatistic(frameProfile, "Navigation", "Cells rebuilt", cellsRebuilt);
        addFrameStatistic(frameProfile, "Navigation", "Rebuild simulated ticks", simulatedTicks);
        breaksSeen = broken.size();
    }

    bool PlatformerConnectionTable::isBuilt(const PlatformerTraversalProfile& profile) const
    {
        return findTable(profile) != nullptr;
    }

    const std::vector<RouteConnection>& PlatformerConnectionTable::connections(
        Cell cell,
        const PlatformerTraversalProfile& profile) const
    {
        return entry(cell, profile).connections;
    }

    const CellRange& PlatformerConnectionTable::footprint(
        Cell cell,
        const PlatformerTraversalProfile& profile) const
    {
        return entry(cell, profile).footprint;
    }

    const PlatformerConnectionTable::ProfileTable* PlatformerConnectionTable::findTable(
        const PlatformerTraversalProfile& profile) const
    {
        const auto table = std::find_if(
            profileTables.begin(),
            profileTables.end(),
            [&profile](const ProfileTable& candidate) { return candidate.profile == profile; });
        return table == profileTables.end() ? nullptr : &*table;
    }

    const PlatformerConnectionTable::CellConnections& PlatformerConnectionTable::entry(
        Cell cell,
        const PlatformerTraversalProfile& profile) const
    {
        const ProfileTable* table = findTable(profile);
        if (table == nullptr)
        {
            throw std::logic_error("The connection table has not built this profile");
        }
        if (!contains(table->grid, cell))
        {
            throw std::out_of_range("The cell is outside the connection table's map");
        }
        return table->cells[indexOf(table->grid, cell)];
    }
}
