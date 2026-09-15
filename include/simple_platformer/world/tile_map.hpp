#pragma once

#include <initializer_list>
#include <string_view>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"

namespace simple_platformer
{
    struct TileDefinition
    {
        bool solid = false;
    };

    class TileMap
    {
    public:
        TileMap(
            int width,
            int height,
            std::vector<int> tiles,
            std::vector<TileDefinition> definitions);

        static TileMap fromAscii(std::initializer_list<std::string_view> rows);

        int width() const;
        int height() const;
        float pixelWidth() const;
        float pixelHeight() const;

        bool contains(GridPosition position) const;
        int tileAt(GridPosition position) const;
        bool isSolid(GridPosition position) const;

        // The map is open above, but its left, right, and bottom edges are walls.
        bool blocksMovement(GridPosition position) const;

    private:
        int mapWidth = 0;
        int mapHeight = 0;
        std::vector<int> tileIds;
        std::vector<TileDefinition> tileDefinitions;
    };
}
