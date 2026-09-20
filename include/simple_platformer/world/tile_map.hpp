#pragma once

#include <map>
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
            const std::map<char, int>& legend = {{'.', 0}, {'#', 1}});

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

    private:
        int mapWidth = 0;
        int mapHeight = 0;
        std::vector<int> tileIds;
        std::vector<TileDefinition> tileDefinitions;
    };
}
