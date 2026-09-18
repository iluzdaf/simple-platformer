#include "tile_catalog.hpp"

#include <cmath>
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
                if (name == "empty")
                {
                    if (definition.blocksMovement || definition.blocksSight)
                    {
                        throw std::invalid_argument("empty must allow movement and sight");
                    }
                }
                else
                {
                    const auto& sprite = value.at("sprite");
                    definition.sprite.position = {
                        sprite.at("x").get<float>(), sprite.at("y").get<float>()};
                    definition.sprite.size = {
                        sprite.at("width").get<float>(), sprite.at("height").get<float>()};
                    const auto position = definition.sprite.position;
                    const auto size = definition.sprite.size;
                    if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
                        !std::isfinite(size.x) || !std::isfinite(size.y) || position.x < 0 ||
                        position.y < 0 || size.x <= 0 || size.y <= 0)
                    {
                        throw std::invalid_argument(
                            "invalid sprite region for tile '" + name + "'");
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
        std::map<char, int> ids;
        for (const auto& entry : legend)
        {
            const auto found = catalog.ids.find(entry.second);
            if (found == catalog.ids.end())
            {
                throw std::invalid_argument(
                    "Unknown tile name '" + entry.second + "' in tileLegend");
            }
            ids.emplace(entry.first, found->second);
        }
        return TileMap::fromAscii(rows, catalog.definitions, ids);
    }
}
