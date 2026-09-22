#include "content_validation.hpp"
#include "content_diagnostics.hpp"

#include <cmath>
#include <cstddef>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/render/sprite.hpp"
#include "tile_catalog.hpp"
#include "level_data.hpp"
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

    void validatePickupSettings(
        const PickupPlacement& placement,
        const std::string& path,
        std::string_view sourceName)
    {
        if (placement.stack.quantity <= 0)
        {
            failJson(
                sourceName,
                fieldPath(path, "quantity"),
                "expected a positive integer, got " + std::to_string(placement.stack.quantity));
        }
    }

    void validateExitSettings(
        const ExitPlacement& placement,
        const std::string& path,
        std::string_view sourceName)
    {
        if (placement.definitionName.empty())
        {
            failJson(
                sourceName, fieldPath(path, "definition"), "exit definition name cannot be empty");
        }
        if (placement.requirement && placement.requirement->quantity <= 0)
        {
            failJson(
                sourceName,
                fieldPath(path, "requirement.quantity"),
                "expected a positive integer, got " +
                    std::to_string(placement.requirement->quantity));
        }
        if (placement.nextLevel && *placement.nextLevel <= 0)
        {
            failJson(sourceName, fieldPath(path, "nextLevel"), "level number must be positive");
        }
    }

    void validateSinglePlacement(
        const std::vector<PlacementOrigin>& origins,
        std::string_view kind,
        std::string_view sourceName)
    {
        if (origins.empty())
        {
            failJson(sourceName, kind, "expected exactly one placement");
        }
        if (origins.size() > 1)
        {
            const auto& duplicate = origins[1];
            const std::string description =
                duplicate.marker ? " marker '" + std::string(1, *duplicate.marker) + "'"
                                 : " placement";
            failJson(
                sourceName,
                duplicate.path,
                "second " + std::string(kind) + description + "; " + std::string(kind) +
                    " already placed at " + origins.front().path);
        }
    }

    void validateTileCatalog(const TileCatalog& catalog)
    {
        if (catalog.tileSize <= 0)
        {
            throw std::invalid_argument("tile catalog needs a positive tileSize");
        }
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
            if (sprite.size != glm::vec2{catalog.tileSize, catalog.tileSize})
            {
                const std::string side = std::to_string(catalog.tileSize);
                std::string message = "tile '" + entry.first + "' sprite must be ";
                message += side;
                message += " by ";
                message += side;
                message += " atlas pixels, the catalogue's tileSize";
                throw std::invalid_argument(message);
            }
            const auto& breaksInto =
                catalog.definitions[static_cast<std::size_t>(id)].breaksIntoTileId;
            if (breaksInto.has_value() &&
                (*breaksInto < 0 ||
                 static_cast<std::size_t>(*breaksInto) >= catalog.definitions.size() ||
                 *breaksInto == id))
            {
                throw std::invalid_argument("invalid breaksInto tile for '" + entry.first + "'");
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
        const std::vector<std::string>& objectSymbols,
        std::string_view sourceName)
    {
        std::set<std::string> tiles;
        for (const auto& symbol : tileSymbols)
        {
            if (symbol.size() != 1)
            {
                failJson(sourceName, "tileLegend", "symbols must be one character");
            }
            if (!tiles.insert(symbol).second)
            {
                failJson(sourceName, fieldPath("tileLegend", symbol), "repeated symbol");
            }
        }
        std::set<std::string> objects;
        for (const auto& symbol : objectSymbols)
        {
            const std::string path = fieldPath("objectLegend", symbol);
            if (symbol.size() != 1)
            {
                failJson(sourceName, path, "symbols must be one character");
            }
            if (tiles.count(symbol) != 0)
            {
                failJson(sourceName, path, "symbol is also defined in tileLegend");
            }
            if (!objects.insert(symbol).second)
            {
                failJson(sourceName, path, "repeated symbol");
            }
        }
    }

    void validateMapRows(
        const std::vector<std::string>& rows,
        const std::map<char, std::string>& legend,
        std::string_view sourceName)
    {
        if (rows.empty())
        {
            failJson(sourceName, "map", "expected at least one row");
        }
        const std::size_t width = rows.front().size();
        if (width == 0)
        {
            failJson(sourceName, "map[0]", "row cannot be empty");
        }
        for (std::size_t row = 0; row < rows.size(); ++row)
        {
            const std::string path = indexPath("map", row);
            if (rows[row].size() != width)
            {
                failJson(
                    sourceName,
                    path,
                    "expected " + std::to_string(width) + " columns, got " +
                        std::to_string(rows[row].size()));
            }
            for (std::size_t column = 0; column < width; ++column)
            {
                const char symbol = rows[row][column];
                if (legend.find(symbol) == legend.end())
                {
                    failJson(
                        sourceName,
                        indexPath(path, column),
                        "unknown symbol '" + std::string(1, symbol) +
                            "'; define it in tileLegend or objectLegend");
                }
            }
        }
    }
}
