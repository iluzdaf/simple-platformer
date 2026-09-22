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
    // Nothing can stand in a tile that blocks movement, so a sight-blocking tile only hides
    // what is in it when it can be walked into. That is the difference between grass, which
    // blocks sight and conceals whoever stands in it, and stone, which only blocks.
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
        // tileSize is the side of one cell in world pixels.
        TileMap(
            int tileSize,
            int width,
            int height,
            std::vector<int> tiles,
            std::vector<TileDefinition> definitions);

        static TileMap fromAscii(
            int tileSize,
            const std::vector<std::string>& rows,
            std::vector<TileDefinition> definitions,
            const std::map<char, int>& legend);

        int tileSize() const;
        int width() const;
        int height() const;
        float pixelWidth() const;
        float pixelHeight() const;

        bool contains(GridPosition cell) const;
        int tileAt(GridPosition cell) const;
        const TileDefinition& definitionAt(GridPosition cell) const;

        // Outside the map, both queries block at the left, right, and bottom.
        // Above the map is open.
        bool blocksMovement(GridPosition cell) const;
        bool blocksSight(GridPosition cell) const;

        // Replaces the cell with whatever its definition breaks into, and reports
        // whether that happened. A cell outside the map, or one whose definition has no
        // breaksIntoTileId, is left alone: callers pass in cells that came from a cast,
        // and map boundaries report as blocking cells that lie outside the map.
        bool breakTile(GridPosition cell);

    private:
        // Row-major offset into tileIds. The cell must be inside the map.
        std::size_t indexOf(GridPosition cell) const;

        int cellSize = 0;
        int mapWidth = 0;
        int mapHeight = 0;
        std::vector<int> tileIds;
        std::vector<TileDefinition> tileDefinitions;
    };
}
