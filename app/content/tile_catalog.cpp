#include "tile_catalog.hpp"
#include "content_diagnostics.hpp"
#include "content_json.hpp"
#include "content_validation.hpp"

#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <map>
#include <stdexcept>
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
        checkJsonFields(root, {"tiles"}, sourceName, "root");
        const auto& tiles = requiredJsonMember(root, "tiles", sourceName, "root");
        checkJsonObject(tiles, sourceName, "tiles");
        if (!tiles.contains("empty"))
        {
            failJson(sourceName, "tiles", "missing 'empty'");
        }
        TileCatalog result;
        // Collected on the way past because a tile may break into one defined further down.
        std::map<std::string, std::string> breaksIntoNames;
        const auto add = [&result, &breaksIntoNames, sourceName](
                             const std::string& name, const nlohmann::json& value)
        {
            const std::string path = fieldPath("tiles", name);
            checkJsonFields(
                value,
                name == "empty"
                    ? std::initializer_list<std::string_view>{"blocksMovement", "blocksSight"}
                    : std::initializer_list<
                          std::
                              string_view>{"blocksMovement", "blocksSight", "sprite", "breaksInto"},
                sourceName,
                path);
            TileDefinition definition;
            definition.blocksMovement = readBoolean(value, "blocksMovement", sourceName, path);
            definition.blocksSight = readBoolean(value, "blocksSight", sourceName, path);
            if (name != "empty")
            {
                const auto& sprite = requiredJsonMember(value, "sprite", sourceName, path);
                const std::string spritePath = fieldPath(path, "sprite");
                checkJsonFields(sprite, {"position", "size"}, sourceName, spritePath);
                definition.sprite = jsonSpriteRegion(sprite, sourceName, spritePath);
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
        // Validation is shared with C++ built catalogues, so it names the tile but not the file.
        try
        {
            validateTileCatalog(result);
        }
        catch (const std::invalid_argument& error)
        {
            failJson(sourceName, {}, error.what());
        }
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
        // Callers can supply catalogues built directly in C++, without using the JSON loader.
        validateTileCatalog(catalog);
        validateTileLegend(legend, catalog);
        std::map<char, int> ids;
        for (const auto& entry : legend)
        {
            ids.emplace(entry.first, catalog.ids.at(entry.second));
        }
        return TileMap::fromAscii(rows, catalog.definitions, ids);
    }
}
