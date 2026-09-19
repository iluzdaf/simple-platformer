#include "content_validation.hpp"

#include <cmath>
#include <cstddef>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "simple_platformer/render/sprite.hpp"
#include "tile_catalog.hpp"
#include "example_level_data.hpp"
#include "simple_platformer/math/validation.hpp"

namespace simple_platformer
{
    void validateContentSprite(const Sprite& sprite)
    {
        if (!isFinite(sprite.region.position) || sprite.region.position.x < 0 ||
            sprite.region.position.y < 0 || !isFinite(sprite.region.size) ||
            sprite.region.size.x <= 0 || sprite.region.size.y <= 0 || !isFinite(sprite.size) ||
            sprite.size.x <= 0 || sprite.size.y <= 0)
        {
            throw std::invalid_argument(
                "sprite requires finite non-negative atlas position and positive sizes");
        }
    }
    void validatePickupSettings(const ExamplePickupPlacement& placement, const std::string& path)
    {
        if (placement.stack.quantity <= 0)
        {
            throw std::invalid_argument(
                path + ".quantity: expected a positive integer, got " +
                std::to_string(placement.stack.quantity));
        }
    }

    void validateExitSettings(const ExampleExitPlacement& placement, const std::string& path)
    {
        if (placement.requirement && placement.requirement->quantity <= 0)
        {
            throw std::invalid_argument(
                path + ".requirement.quantity: expected a positive integer, got " +
                std::to_string(placement.requirement->quantity));
        }
        if (placement.nextLevel && *placement.nextLevel <= 0)
        {
            throw std::invalid_argument(path + ".nextLevel: level number must be positive");
        }
    }

    void validateSinglePlacement(const std::vector<PlacementOrigin>& origins, std::string_view kind)
    {
        if (origins.empty())
        {
            throw std::invalid_argument(std::string(kind) + ": expected exactly one placement");
        }
        if (origins.size() > 1)
        {
            const auto& duplicate = origins[1];
            const std::string description =
                duplicate.marker ? " marker '" + std::string(1, *duplicate.marker) + "'"
                                 : " placement";
            throw std::invalid_argument(
                duplicate.path + ": second " + std::string(kind) + description + "; " +
                std::string(kind) + " already placed at " + origins.front().path);
        }
    }

    void validateTileCatalog(const TileCatalog& catalog)
    {
        const auto empty = catalog.ids.find("empty");
        if (empty == catalog.ids.end() || empty->second != 0 || catalog.definitions.empty())
        {
            throw std::invalid_argument("tile catalog must reserve ID zero for 'empty'");
        }
        if (catalog.definitions.front().blocksMovement || catalog.definitions.front().blocksSight)
        {
            throw std::invalid_argument("empty must allow movement and sight");
        }
        std::set<int> usedIds;
        for (const auto& entry : catalog.ids)
        {
            const int id = entry.second;
            if (id < 0 || static_cast<std::size_t>(id) >= catalog.definitions.size() ||
                !usedIds.insert(id).second)
            {
                throw std::invalid_argument(
                    "invalid or repeated tile ID for '" + entry.first + "'");
            }
            if (id == 0)
            {
                // Empty tiles are not drawn, so they do not need a sprite region.
                continue;
            }
            const auto& sprite = catalog.definitions[static_cast<std::size_t>(id)].sprite;
            if (!std::isfinite(sprite.position.x) || !std::isfinite(sprite.position.y) ||
                !std::isfinite(sprite.size.x) || !std::isfinite(sprite.size.y) ||
                sprite.position.x < 0 || sprite.position.y < 0 || sprite.size.x <= 0 ||
                sprite.size.y <= 0)
            {
                throw std::invalid_argument("invalid sprite region for tile '" + entry.first + "'");
            }
        }
        if (usedIds.size() != catalog.definitions.size())
        {
            throw std::invalid_argument("every tile definition must have a catalogue name");
        }
    }

    void validateTileLegend(const std::map<char, std::string>& legend, const TileCatalog& catalog)
    {
        if (legend.empty())
        {
            throw std::invalid_argument("tileLegend: expected at least one symbol");
        }
        for (const auto& entry : legend)
        {
            if (catalog.ids.find(entry.second) == catalog.ids.end())
            {
                throw std::invalid_argument(
                    "Unknown tile name '" + entry.second + "' in tileLegend");
            }
        }
    }

    void validateLegendSymbols(
        const std::vector<std::string>& tileSymbols,
        const std::vector<std::string>& objectSymbols)
    {
        std::set<std::string> tiles;
        for (const auto& symbol : tileSymbols)
        {
            if (symbol.size() != 1)
            {
                throw std::invalid_argument("tileLegend: symbols must be one character");
            }
            if (!tiles.insert(symbol).second)
            {
                throw std::invalid_argument("tileLegend." + symbol + ": repeated symbol");
            }
        }
        std::set<std::string> objects;
        for (const auto& symbol : objectSymbols)
        {
            const std::string path = "objectLegend." + symbol + ": ";
            if (symbol.size() != 1)
            {
                throw std::invalid_argument(path + "symbols must be one character");
            }
            if (tiles.count(symbol) != 0)
            {
                throw std::invalid_argument(path + "symbol is also defined in tileLegend");
            }
            if (!objects.insert(symbol).second)
            {
                throw std::invalid_argument(path + "repeated symbol");
            }
        }
    }

    void validateMapRows(
        const std::vector<std::string>& rows,
        const std::map<char, std::string>& legend)
    {
        if (rows.empty())
        {
            throw std::invalid_argument("map: expected at least one row");
        }
        const std::size_t width = rows.front().size();
        if (width == 0)
        {
            throw std::invalid_argument("map[0]: row cannot be empty");
        }
        for (std::size_t row = 0; row < rows.size(); ++row)
        {
            const std::string path = "map[" + std::to_string(row) + "]";
            if (rows[row].size() != width)
            {
                throw std::invalid_argument(
                    path + ": expected " + std::to_string(width) + " columns, got " +
                    std::to_string(rows[row].size()));
            }
            for (std::size_t column = 0; column < width; ++column)
            {
                const char symbol = rows[row][column];
                if (legend.find(symbol) == legend.end())
                {
                    throw std::invalid_argument(
                        path + "[" + std::to_string(column) + "]: unknown symbol '" +
                        std::string(1, symbol) + "'; define it in tileLegend or objectLegend");
                }
            }
        }
    }
}
