#include "tile_catalog.hpp"
#include "content_validation.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
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
        try
        {
            const auto root = nlohmann::json::parse(text);
            const auto& tiles = root.at("tiles");
            if (!tiles.is_object() || !tiles.contains("empty"))
            {
                throw std::invalid_argument("tiles must be an object containing 'empty'");
            }
            TileCatalog result;
            const auto add = [&result](const std::string& name, const nlohmann::json& value)
            {
                TileDefinition definition;
                definition.blocksMovement = value.at("blocksMovement").get<bool>();
                definition.blocksSight = value.at("blocksSight").get<bool>();
                if (name != "empty")
                {
                    const auto& sprite = value.at("sprite");
                    definition.sprite.position = {
                        sprite.at("x").get<float>(), sprite.at("y").get<float>()};
                    definition.sprite.size = {
                        sprite.at("width").get<float>(), sprite.at("height").get<float>()};
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
            validateTileCatalog(result);
            return result;
        }
        catch (const nlohmann::json::exception& error)
        {
            throw std::invalid_argument(std::string(sourceName) + ": " + error.what());
        }
        catch (const std::invalid_argument& error)
        {
            throw std::invalid_argument(std::string(sourceName) + ": " + error.what());
        }
    }

    TileCatalog loadTileCatalog(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        if (!file)
        {
            throw std::invalid_argument("Could not open tile catalog '" + path.string() + "'");
        }
        const std::string text{
            std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        return parseTileCatalog(text, path.string());
    }

    TileMap makeTileMap(
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
