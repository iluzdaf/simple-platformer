#include "simple_platformer/world/tile_map.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"

namespace simple_platformer
{
    TileMap::TileMap(
        int tileSize,
        int width,
        int height,
        std::vector<int> tiles,
        std::vector<TileDefinition> definitions)
        : cellSize(tileSize),
          mapWidth(width),
          mapHeight(height),
          tileIds(std::move(tiles)),
          tileDefinitions(std::move(definitions))
    {
        if (cellSize <= 0)
        {
            throw std::invalid_argument("A tile map must have a positive tile size");
        }
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

        if (tileDefinitions.empty() || tileDefinitions.front().blocksMovement ||
            tileDefinitions.front().blocksSight)
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

    TileMap TileMap::fromAscii(
        int tileSize,
        const std::vector<std::string>& rows,
        std::vector<TileDefinition> definitions,
        const std::map<char, int>& legend)
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
                const auto entry = legend.find(symbol);
                if (entry == legend.end())
                {
                    throw std::invalid_argument("An ASCII tile map contains an unknown symbol");
                }
                tiles.push_back(entry->second);
            }
        }

        return TileMap(
            tileSize,
            mapWidth,
            static_cast<int>(rows.size()),
            std::move(tiles),
            std::move(definitions));
    }

    int TileMap::width() const
    {
        return mapWidth;
    }

    int TileMap::height() const
    {
        return mapHeight;
    }

    int TileMap::tileSize() const
    {
        return cellSize;
    }

    float TileMap::pixelWidth() const
    {
        return static_cast<float>(mapWidth * cellSize);
    }

    float TileMap::pixelHeight() const
    {
        return static_cast<float>(mapHeight * cellSize);
    }

    bool TileMap::contains(GridPosition cell) const
    {
        return cell.x >= 0 && cell.x < mapWidth && cell.y >= 0 && cell.y < mapHeight;
    }

    int TileMap::tileAt(GridPosition cell) const
    {
        if (!contains(cell))
        {
            throw std::out_of_range("Cell is outside the map");
        }

        return tileIds[indexOf(cell)];
    }

    const TileDefinition& TileMap::definitionAt(GridPosition cell) const
    {
        const int tileId = tileAt(cell);
        return tileDefinitions[static_cast<std::size_t>(tileId)];
    }

    bool TileMap::blocksSight(GridPosition cell) const
    {
        return contains(cell) ? definitionAt(cell).blocksSight : blocksMovement(cell);
    }

    bool TileMap::blocksMovement(GridPosition cell) const
    {
        if (cell.x < 0 || cell.x >= mapWidth)
        {
            return true;
        }

        if (cell.y < 0)
        {
            return false;
        }

        if (cell.y >= mapHeight)
        {
            return true;
        }

        return definitionAt(cell).blocksMovement;
    }

    bool TileMap::breakTile(GridPosition cell)
    {
        if (!contains(cell))
        {
            return false;
        }

        const std::optional<int> broken = definitionAt(cell).breaksIntoTileId;
        if (!broken.has_value())
        {
            return false;
        }

        tileIds[indexOf(cell)] = *broken;
        brokenCellLog.push_back(cell);
        return true;
    }

    const std::vector<GridPosition>& TileMap::brokenCells() const
    {
        return brokenCellLog;
    }

    std::size_t TileMap::indexOf(GridPosition cell) const
    {
        return static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(mapWidth) +
               static_cast<std::size_t>(cell.x);
    }
}
