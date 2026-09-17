#include "simple_platformer/world/tile_map.hpp"

#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"

namespace simple_platformer
{
    TileMap::TileMap(
        int width,
        int height,
        std::vector<int> tiles,
        std::vector<TileDefinition> definitions)
        : mapWidth(width), mapHeight(height), tileIds(std::move(tiles)),
          tileDefinitions(std::move(definitions))
    {
        if (mapWidth <= 0 || mapHeight <= 0)
        {
            throw std::invalid_argument("A tile map must have positive dimensions");
        }

        const std::size_t expectedTileCount =
            static_cast<std::size_t>(mapWidth) * static_cast<std::size_t>(mapHeight);
        if (tileIds.size() != expectedTileCount)
        {
            throw std::invalid_argument("Tile count does not match the map dimensions");
        }

        if (tileDefinitions.empty() || tileDefinitions.front().solid)
        {
            throw std::invalid_argument("Tile ID zero must be defined as empty");
        }

        for (const int tile : tileIds)
        {
            if (tile < 0 || static_cast<std::size_t>(tile) >= tileDefinitions.size())
            {
                throw std::invalid_argument("A tile map contains an undefined tile ID");
            }
        }
    }

    TileMap TileMap::fromAscii(std::initializer_list<std::string_view> rows)
    {
        std::vector<std::string> ownedRows;
        ownedRows.reserve(rows.size());
        for (const std::string_view row : rows)
        {
            ownedRows.emplace_back(row);
        }
        return fromAscii(ownedRows, {{false}, {true, {{0.0F, 0.0F}, {1.0F, 1.0F}}}});
    }

    TileMap TileMap::fromAscii(
        const std::vector<std::string>& rows,
        std::vector<TileDefinition> definitions)
    {
        if (rows.empty())
        {
            throw std::invalid_argument("An ASCII tile map needs at least one row");
        }

        const int mapWidth = static_cast<int>(rows.front().size());
        if (mapWidth == 0)
        {
            throw std::invalid_argument("An ASCII tile map cannot have empty rows");
        }

        std::vector<int> tiles;
        tiles.reserve(static_cast<std::size_t>(mapWidth) * rows.size());

        for (const std::string& row : rows)
        {
            if (static_cast<int>(row.size()) != mapWidth)
            {
                throw std::invalid_argument("Every ASCII tile map row must have the same width");
            }

            for (const char symbol : row)
            {
                if (symbol == '.')
                {
                    tiles.push_back(0);
                }
                else if (symbol == '#')
                {
                    tiles.push_back(1);
                }
                else
                {
                    throw std::invalid_argument("An ASCII tile map contains an unknown symbol");
                }
            }
        }

        return TileMap(
            mapWidth, static_cast<int>(rows.size()), std::move(tiles), std::move(definitions));
    }

    int TileMap::width() const
    {
        return mapWidth;
    }

    int TileMap::height() const
    {
        return mapHeight;
    }

    float TileMap::pixelWidth() const
    {
        return static_cast<float>(mapWidth * TileSize);
    }

    float TileMap::pixelHeight() const
    {
        return static_cast<float>(mapHeight * TileSize);
    }

    bool TileMap::contains(GridPosition position) const
    {
        return position.x >= 0 && position.x < mapWidth && position.y >= 0 &&
               position.y < mapHeight;
    }

    int TileMap::tileAt(GridPosition position) const
    {
        if (!contains(position))
        {
            throw std::out_of_range("Tile position is outside the map");
        }

        const std::size_t index =
            static_cast<std::size_t>(position.y) * static_cast<std::size_t>(mapWidth) +
            static_cast<std::size_t>(position.x);
        return tileIds[index];
    }

    const TileDefinition& TileMap::definitionAt(GridPosition position) const
    {
        const int tileId = tileAt(position);
        return tileDefinitions[static_cast<std::size_t>(tileId)];
    }

    bool TileMap::isSolid(GridPosition position) const
    {
        return definitionAt(position).solid;
    }

    bool TileMap::blocksMovement(GridPosition position) const
    {
        if (position.x < 0 || position.x >= mapWidth)
        {
            return true;
        }

        if (position.y < 0)
        {
            return false;
        }

        if (position.y >= mapHeight)
        {
            return true;
        }

        return isSolid(position);
    }
}
