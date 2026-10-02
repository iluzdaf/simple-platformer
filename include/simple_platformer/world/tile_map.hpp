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
    // Shared rules and art for a tile ID. Movement and sight blocking are independent:
    // glass stops bodies but not sight; grass stops sight but lets bodies enter it.
    struct TileDefinition
    {
        bool blocksMovement = false;
        bool blocksSight = false;
        SpriteRegion sprite;
        // The tile this one becomes when broken. Unset means nothing breaks it.
        std::optional<int> breaksIntoTileId = std::nullopt;
        bool climbable = false;
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
        GridSize size() const;
        float pixelWidth() const;
        float pixelHeight() const;

        bool contains(Cell cell) const;
        int tileAt(Cell cell) const;
        const TileDefinition& definitionAt(Cell cell) const;

        // Outside the map, both queries block at the left, right, and bottom.
        // Above the map is open.
        bool blocksMovement(Cell cell) const;
        bool blocksSight(Cell cell) const;
        // Out-of-map walls are not climbable.
        bool climbableAt(Cell cell) const;

        // Replaces the tile with breaksIntoTileId and returns true. Returns false for an
        // unbreakable tile or an off-map cell, including boundaries reported by a cast.
        bool breakTile(Cell cell);
        // Successful breaks in order. Navigation remembers how far it has read this log
        // so it can update connections and paths after a break.
        const std::vector<Cell>& brokenCells() const;

    private:
        // Row-major offset into tileIds. The cell must be inside the map.
        std::size_t indexOf(Cell cell) const;

        int mapTileSize = 0;
        int mapWidth = 0;
        int mapHeight = 0;
        std::vector<int> tileIds;
        std::vector<TileDefinition> tileDefinitions;
        std::vector<Cell> brokenCellLog;
    };
}
