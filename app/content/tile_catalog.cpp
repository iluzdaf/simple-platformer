#include "tile_catalog.hpp"
#include "content_diagnostics.hpp"
#include "content_json.hpp"
#include "content_validation.hpp"

#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    TileCatalog parseTileCatalog(std::string_view text, std::string_view sourceName)
    {
        const auto root = parseContentRoot(text, sourceName);
        checkJsonFields(root, {"tileSize", "tiles"}, sourceName, "root");
        const auto& tiles = requiredJsonMember(root, "tiles", sourceName, "root");
        checkJsonObject(tiles, sourceName, "tiles");
        if (!tiles.contains("empty"))
        {
            failJson(sourceName, "tiles", "missing 'empty'");
        }
        TileCatalog result;
        result.tileSize = jsonInteger(
            requiredJsonMember(root, "tileSize", sourceName, "root"), sourceName, "tileSize");
        // Resolve breaksInto after every tile has an ID, so forward references work.
        std::map<std::string, std::string> breaksIntoNames;
        const auto add = [&result, &breaksIntoNames, sourceName](
                             const std::string& name, const nlohmann::json& value)
        {
            const std::string path = fieldPath("tiles", name);
            if (name == "empty")
            {
                checkJsonFields(
                    value, {"blocksMovement", "blocksSight", "climbable"}, sourceName, path);
            }
            else
            {
                checkJsonFields(
                    value,
                    {"blocksMovement", "blocksSight", "climbable", "sprite", "breaksInto"},
                    sourceName,
                    path);
            }
            TileDefinition definition;
            definition.blocksMovement = readBoolean(value, "blocksMovement", sourceName, path);
            definition.blocksSight = readBoolean(value, "blocksSight", sourceName, path);
            readOptionalBoolean(value, "climbable", definition.climbable, sourceName, path);
            if (name != "empty")
            {
                const auto& sprite = requiredJsonMember(value, "sprite", sourceName, path);
                const std::string spritePath = fieldPath(path, "sprite");
                checkJsonFields(sprite, {"position"}, sourceName, spritePath);
                const auto side = static_cast<float>(result.tileSize);
                definition.sprite = {
                    jsonVector(
                        requiredJsonMember(sprite, "position", sourceName, spritePath),
                        sourceName,
                        fieldPath(spritePath, "position")),
                    {side, side}};
                std::string breaksInto;
                readOptionalText(value, "breaksInto", breaksInto, sourceName, path);
                if (!breaksInto.empty())
                {
                    breaksIntoNames.emplace(name, breaksInto);
                }
            }
            result.ids.emplace(name, static_cast<int>(result.definitions.size()));
            result.definitions.push_back(definition);
        };
        add("empty", tiles.at("empty"));
        for (const auto& entry : tiles.items())
        {
            if (entry.key() != "empty")
            {
                add(entry.key(), entry.value());
            }
        }
        for (const auto& entry : breaksIntoNames)
        {
            const std::string path = fieldPath(fieldPath("tiles", entry.first), "breaksInto");
            const auto target = result.ids.find(entry.second);
            if (target == result.ids.end())
            {
                failJson(sourceName, path, "unknown tile name '" + entry.second + "'");
            }
            const auto broken = static_cast<std::size_t>(result.ids.at(entry.first));
            result.definitions[broken].breaksIntoTileId = target->second;
        }
        validateInFile(sourceName, [&] { validateTileCatalog(result); });
        return result;
    }

    TileCatalog loadTileCatalog(const std::filesystem::path& path)
    {
        return parseTileCatalog(loadContentText(path), path.string());
    }

    TileMap composeTileMap(
        const std::vector<std::string>& rows,
        const std::map<char, std::string>& legend,
        const TileCatalog& catalog)
    {
        // Callers can supply catalogs built directly in C++, without using the JSON loader.
        validateTileCatalog(catalog);
        validateTileLegend(legend, catalog);
        std::map<char, int> ids;
        for (const auto& entry : legend)
        {
            ids.emplace(entry.first, catalog.ids.at(entry.second));
        }
        return TileMap::fromAscii(catalog.tileSize, rows, catalog.definitions, ids);
    }

    void validateTileAtlasRegions(
        const TileCatalog& catalog,
        glm::ivec2 atlasSize,
        std::string_view sourceName)
    {
        for (const auto& [name, id] : catalog.ids)
        {
            // Empty tiles are not drawn.
            if (id != 0)
            {
                requireInAtlas(
                    catalog.definitions[static_cast<std::size_t>(id)].sprite,
                    atlasSize,
                    sourceName,
                    fieldPath(fieldPath("tiles", name), "sprite"));
            }
        }
    }
}
