#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    struct TileDefinition
    {
        bool blocksMovement = false;
        bool blocksSight = false;
        SpriteRegion sprite;
        // The tile this one becomes when broken. Unset means nothing breaks it.
        std::optional<int> breaksIntoTileId = std::nullopt;
    };

    class TileMap
    {
    public:
        TileMap(
            int width,
            int height,
            std::vector<int> tiles,
            std::vector<TileDefinition> definitions);

        static TileMap fromAscii(
            const std::vector<std::string>& rows,
            std::vector<TileDefinition> definitions,
            const std::map<char, int>& legend);

        int width() const;
        int height() const;
        float pixelWidth() const;
        float pixelHeight() const;

        bool contains(GridPosition position) const;
        int tileAt(GridPosition position) const;
        const TileDefinition& definitionAt(GridPosition position) const;

        // Outside the map, both queries block at the left, right, and bottom.
        // Above the map is open.
        bool blocksMovement(GridPosition position) const;
        bool blocksSight(GridPosition position) const;

        // Replaces the cell with whatever its definition breaks into, and reports
        // whether that happened. A cell outside the map, or one whose definition has no
        // breaksIntoTileId, is left alone: callers pass in cells that came from a cast,
        // and map boundaries report as blocking cells that lie outside the map.
        bool breakTile(GridPosition position);

    private:
        // Row-major offset into tileIds. The position must be inside the map.
        std::size_t indexOf(GridPosition position) const;

        int mapWidth = 0;
        int mapHeight = 0;
        std::vector<int> tileIds;
        std::vector<TileDefinition> tileDefinitions;
    };
}
