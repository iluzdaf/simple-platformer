#include "example_level_data.hpp"
#include "content_validation.hpp"

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "game/item_catalog.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"

namespace simple_platformer
{
    namespace
    {
        using Json = nlohmann::json;

        [[noreturn]] void fail(
            std::string_view sourceName,
            std::string_view path,
            std::string_view message)
        {
            throw std::invalid_argument(
                std::string(sourceName) + ": " + std::string(path) + ": " + std::string(message));
        }

        const Json& member(
            const Json& object,
            std::string_view key,
            std::string_view sourceName,
            std::string_view path)
        {
            if (!object.is_object())
            {
                fail(sourceName, path, "expected an object");
            }
            const auto found = object.find(std::string(key));
            if (found == object.end())
            {
                fail(sourceName, path, "missing '" + std::string(key) + "'");
            }
            return *found;
        }

        int integer(const Json& value, std::string_view sourceName, std::string_view path)
        {
            if (!value.is_number_integer())
            {
                fail(sourceName, path, "expected an integer");
            }
            try
            {
                return value.get<int>();
            }
            catch (const Json::out_of_range&)
            {
                fail(sourceName, path, "integer is outside the supported range");
            }
        }

        float number(const Json& value, std::string_view sourceName, std::string_view path)
        {
            if (!value.is_number())
            {
                fail(sourceName, path, "expected a number");
            }
            const float result = value.get<float>();
            if (!std::isfinite(result))
            {
                fail(sourceName, path, "number must be finite");
            }
            return result;
        }

        bool boolean(const Json& value, std::string_view sourceName, std::string_view path)
        {
            if (!value.is_boolean())
            {
                fail(sourceName, path, "expected true or false");
            }
            return value.get<bool>();
        }

        std::string text(const Json& value, std::string_view sourceName, std::string_view path)
        {
            if (!value.is_string())
            {
                fail(sourceName, path, "expected text");
            }
            return value.get<std::string>();
        }

        glm::vec2 vector2(const Json& value, std::string_view sourceName, std::string_view path)
        {
            if (!value.is_array() || value.size() != 2)
            {
                fail(sourceName, path, "expected [x, y]");
            }
            return {
                number(value[0], sourceName, std::string(path) + "[0]"),
                number(value[1], sourceName, std::string(path) + "[1]")};
        }

        GridPosition gridPosition(
            const Json& value,
            std::string_view sourceName,
            std::string_view path)
        {
            if (!value.is_array() || value.size() != 2)
            {
                fail(sourceName, path, "expected [column, row]");
            }
            return {
                integer(value[0], sourceName, std::string(path) + "[0]"),
                integer(value[1], sourceName, std::string(path) + "[1]")};
        }

        glm::vec2 feetPosition(
            const Json& object,
            std::string_view cellKey,
            std::string_view feetKey,
            std::string_view sourceName,
            std::string_view path)
        {
            if (!object.is_object())
            {
                fail(sourceName, path, "expected an object");
            }
            const auto cell = object.find(std::string(cellKey));
            const auto feet = object.find(std::string(feetKey));
            if ((cell == object.end()) == (feet == object.end()))
            {
                fail(
                    sourceName,
                    path,
                    "supply exactly one of '" + std::string(cellKey) + "' or '" +
                        std::string(feetKey) + "'");
            }
            if (cell != object.end())
            {
                return navigationFeet(gridPosition(
                    *cell, sourceName, std::string(path) + "." + std::string(cellKey)));
            }
            return vector2(*feet, sourceName, std::string(path) + "." + std::string(feetKey));
        }

        std::string itemName(const Json& value, std::string_view sourceName, std::string_view path)
        {
            const std::string name = text(value, sourceName, path);
            if (name.empty())
            {
                fail(sourceName, path, "item name cannot be empty");
            }
            return name;
        }

        std::string actorDefinitionName(
            const Json& value,
            std::string_view sourceName,
            std::string_view path)
        {
            const std::string name = text(value, sourceName, path);
            if (name.empty())
            {
                fail(sourceName, path, "actor definition name cannot be empty");
            }
            return name;
        }

        Patrol patrol(const Json& value, std::string_view sourceName, std::string_view path)
        {
            return {
                feetPosition(value, "firstCell", "firstFeet", sourceName, path),
                feetPosition(value, "secondCell", "secondFeet", sourceName, path),
                true};
        }

        ExampleActorPlacement actor(
            const Json& value,
            std::string_view sourceName,
            const std::string& path)
        {
            ExampleActorPlacement result;
            result.definitionName = actorDefinitionName(
                member(value, "definition", sourceName, path), sourceName, path + ".definition");
            result.spawnFeet = feetPosition(value, "spawnCell", "spawnFeet", sourceName, path);
            const auto found = value.find("patrol");
            if (found != value.end())
            {
                result.patrol = patrol(*found, sourceName, path + ".patrol");
            }
            return result;
        }

        ExamplePickupPlacement pickup(
            const Json& value,
            std::string_view sourceName,
            const std::string& path)
        {
            if (value.contains("definition"))
            {
                if (value.contains("item") || value.contains("quantity"))
                {
                    fail(
                        sourceName,
                        path,
                        "use either a pickup definition or an inline item and quantity");
                }
                ExamplePickupPlacement result;
                result.spawnFeet = feetPosition(value, "spawnCell", "spawnFeet", sourceName, path);
                result.definitionName =
                    text(value.at("definition"), sourceName, path + ".definition");
                if (result.definitionName.empty())
                {
                    fail(
                        sourceName, path + ".definition", "pickup definition name cannot be empty");
                }
                return result;
            }
            const int quantity = integer(
                member(value, "quantity", sourceName, path), sourceName, path + ".quantity");
            const ExamplePickupPlacement result{
                feetPosition(value, "spawnCell", "spawnFeet", sourceName, path),
                {itemName(member(value, "item", sourceName, path), sourceName, path + ".item"),
                 quantity}};
            try
            {
                validatePickupSettings(result, path);
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument(std::string(sourceName) + ": " + error.what());
            }
            return result;
        }

        ExampleExitPlacement levelExit(
            const Json& value,
            std::string_view sourceName,
            const std::string& path = "exit")
        {
            ExampleExitPlacement result;
            result.definitionName = text(
                member(value, "definition", sourceName, path), sourceName, path + ".definition");
            result.spawnFeet = feetPosition(value, "spawnCell", "spawnFeet", sourceName, path);

            if (const auto found = value.find("requirement"); found != value.end())
            {
                const Json& requirement = *found;
                const int quantity = integer(
                    member(requirement, "quantity", sourceName, path + ".requirement"),
                    sourceName,
                    path + ".requirement.quantity");
                result.requirement = NamedItemStack{
                    itemName(
                        member(requirement, "item", sourceName, path + ".requirement"),
                        sourceName,
                        path + ".requirement.item"),
                    quantity};
            }

            if (const auto found = value.find("consumeItem"); found != value.end())
            {
                result.consumeItem = boolean(*found, sourceName, path + ".consumeItem");
            }
            if (const auto found = value.find("nextLevel"); found != value.end())
            {
                const int nextLevel = integer(*found, sourceName, path + ".nextLevel");
                result.nextLevel = nextLevel;
            }
            try
            {
                validateExitSettings(result, path);
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument(std::string(sourceName) + ": " + error.what());
            }
            return result;
        }

        std::vector<std::string> mapRows(
            const Json& value,
            std::string_view sourceName,
            const std::map<char, std::string>& legend)
        {
            if (!value.is_array() || value.empty())
            {
                fail(sourceName, "map", "expected at least one row");
            }

            std::vector<std::string> rows;
            rows.reserve(value.size());
            for (std::size_t rowIndex = 0; rowIndex < value.size(); ++rowIndex)
            {
                const std::string path = "map[" + std::to_string(rowIndex) + "]";
                rows.push_back(text(value[rowIndex], sourceName, path));
            }
            try
            {
                validateMapRows(rows, legend);
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument(std::string(sourceName) + ": " + error.what());
            }
            return rows;
        }

        void validateJsonLegendSymbols(
            const Json& tiles,
            const Json& objects,
            std::string_view sourceName)
        {
            std::vector<std::string> tileSymbols;
            std::vector<std::string> objectSymbols;
            for (const auto& entry : tiles.items())
            {
                tileSymbols.push_back(entry.key());
            }
            for (const auto& entry : objects.items())
            {
                objectSymbols.push_back(entry.key());
            }
            try
            {
                validateLegendSymbols(tileSymbols, objectSymbols);
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument(std::string(sourceName) + ": " + error.what());
            }
        }

        void validatePlacementOrigins(
            const std::vector<PlacementOrigin>& origins,
            std::string_view kind,
            std::string_view sourceName)
        {
            try
            {
                validateSinglePlacement(origins, kind);
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument(std::string(sourceName) + ": " + error.what());
            }
        }

        std::vector<PlacementOrigin> playerOrigins(const Json& root)
        {
            std::vector<PlacementOrigin> result;
            for (const auto* key : {"playerSpawnCell", "playerSpawnFeet"})
            {
                if (root.contains(key))
                {
                    result.push_back({key, std::nullopt});
                }
            }
            return result;
        }

        // Expand the authoring shorthand into the existing placement format, so both
        // ways of placing objects use the same parsing and validation below.
        Json expandObjectLegend(Json root, std::string_view sourceName)
        {
            if (!root.contains("objectLegend"))
            {
                return root;
            }
            const Json legend = root.at("objectLegend");
            if (!legend.is_object())
            {
                fail(sourceName, "objectLegend", "expected an object");
            }
            if (!root.contains("tileLegend"))
            {
                root["tileLegend"] = {{".", "empty"}, {"#", "stone"}};
            }
            if (!root.at("tileLegend").is_object() || root.at("tileLegend").empty())
            {
                fail(sourceName, "tileLegend", "expected a nonempty object");
            }
            validateJsonLegendSymbols(root.at("tileLegend"), legend, sourceName);
            for (const auto& entry : legend.items())
            {
                const std::string path = "objectLegend." + entry.key();
                const Json& object = entry.value();
                const std::string type =
                    text(member(object, "type", sourceName, path), sourceName, path + ".type");
                if (object.contains("spawnCell") || object.contains("spawnFeet"))
                {
                    fail(sourceName, path, "position comes from the map symbol");
                }
                // Validate unused definitions too; the actual cell is supplied when scanning.
                Json placement = object;
                placement["spawnCell"] = {0, 0};
                if (type == "pickup")
                {
                    pickup(placement, sourceName, path);
                }
                else if (type == "exit")
                {
                    levelExit(placement, sourceName, path);
                }
                else if (type == "actor")
                {
                    actor(placement, sourceName, path);
                }
                else if (type != "player")
                {
                    fail(
                        sourceName,
                        path + ".type",
                        "unknown object type '" + type +
                            "'; expected player, actor, pickup, or exit");
                }
                // The marker creates an object, not a terrain tile.
                root["tileLegend"][entry.key()] = "empty";
            }
            for (const auto* key : {"actors", "pickups"})
            {
                if (!root.contains(key))
                {
                    root[key] = Json::array();
                }
                if (!root.at(key).is_array())
                {
                    fail(sourceName, key, "expected an array");
                }
            }
            const Json& rows = member(root, "map", sourceName, "root");
            if (!rows.is_array())
            {
                fail(sourceName, "map", "expected an array");
            }
            auto players = playerOrigins(root);
            std::vector<PlacementOrigin> exits;
            if (root.contains("exit"))
            {
                exits.push_back({"exit", std::nullopt});
            }
            for (std::size_t row = 0; row < rows.size(); ++row)
            {
                const std::string cells =
                    text(rows[row], sourceName, "map[" + std::to_string(row) + "]");
                for (std::size_t column = 0; column < cells.size(); ++column)
                {
                    const auto found = legend.find(std::string(1, cells[column]));
                    if (found == legend.end())
                    {
                        continue;
                    }
                    Json placement = *found;
                    const std::string type = placement.at("type").get<std::string>();
                    placement["spawnCell"] = {column, row};
                    const std::string location =
                        "map[" + std::to_string(row) + "][" + std::to_string(column) + "]";
                    if (type == "player")
                    {
                        players.push_back({location, cells[column]});
                        validatePlacementOrigins(players, "player", sourceName);
                        root["playerSpawnCell"] = placement.at("spawnCell");
                    }
                    else if (type == "exit")
                    {
                        exits.push_back({location, cells[column]});
                        validatePlacementOrigins(exits, "exit", sourceName);
                        root["exit"] = std::move(placement);
                    }
                    else
                    {
                        root[type == "pickup" ? "pickups" : "actors"].push_back(
                            std::move(placement));
                    }
                }
            }
            return root;
        }

        ExampleLevelData levelData(const Json& root, std::string_view sourceName)
        {
            ExampleLevelData result;
            if (root.contains("tileLegend"))
            {
                const auto& legend = root.at("tileLegend");
                if (!legend.is_object() || legend.empty())
                {
                    fail(sourceName, "tileLegend", "expected a nonempty object");
                }
                result.tileLegend.clear();
                validateJsonLegendSymbols(legend, Json::object(), sourceName);
                for (const auto& entry : legend.items())
                {
                    result.tileLegend.emplace(
                        entry.key().front(), text(entry.value(), sourceName, "tileLegend"));
                }
            }
            result.mapRows =
                mapRows(member(root, "map", sourceName, "root"), sourceName, result.tileLegend);
            validatePlacementOrigins(playerOrigins(root), "player", sourceName);
            const std::vector<PlacementOrigin> exits =
                root.contains("exit") ? std::vector<PlacementOrigin>{{"exit", std::nullopt}}
                                      : std::vector<PlacementOrigin>{};
            validatePlacementOrigins(exits, "exit", sourceName);
            result.playerSpawnFeet =
                feetPosition(root, "playerSpawnCell", "playerSpawnFeet", sourceName, "root");

            const Json& actors = member(root, "actors", sourceName, "root");
            if (!actors.is_array())
            {
                fail(sourceName, "actors", "expected an array");
            }
            result.actors.reserve(actors.size());
            if (root.contains("objectLegend"))
            {
                for (const auto& entry : root.at("objectLegend").items())
                {
                    const auto type = entry.value().at("type").get<std::string>();
                    const auto origin = "objectLegend." + entry.key();
                    if (type == "pickup")
                    {
                        const auto& value = entry.value();
                        if (value.contains("definition"))
                        {
                            result.pickupReferences.emplace(
                                origin + ".definition", value.at("definition").get<std::string>());
                        }
                        else
                        {
                            result.itemReferences.emplace(
                                origin + ".item", value.at("item").get<std::string>());
                        }
                    }
                    if (type == "exit")
                    {
                        result.exitReferences.emplace(
                            origin + ".definition",
                            entry.value().at("definition").get<std::string>());
                    }
                    if (type == "exit" && entry.value().contains("requirement"))
                    {
                        result.itemReferences.emplace(
                            origin + ".requirement.item",
                            entry.value().at("requirement").at("item").get<std::string>());
                    }
                    if (type == "actor")
                    {
                        result.actorReferences.emplace(
                            "objectLegend." + entry.key() + ".definition",
                            entry.value().at("definition").get<std::string>());
                    }
                }
            }
            for (std::size_t index = 0; index < actors.size(); ++index)
            {
                result.actors.push_back(
                    actor(actors[index], sourceName, "actors[" + std::to_string(index) + "]"));
                result.actorReferences.emplace(
                    "actors[" + std::to_string(index) + "].definition",
                    result.actors.back().definitionName);
            }

            const Json& pickups = member(root, "pickups", sourceName, "root");
            if (!pickups.is_array())
            {
                fail(sourceName, "pickups", "expected an array");
            }
            result.pickups.reserve(pickups.size());
            for (std::size_t index = 0; index < pickups.size(); ++index)
            {
                result.pickups.push_back(
                    pickup(pickups[index], sourceName, "pickups[" + std::to_string(index) + "]"));
                const auto& placement = result.pickups.back();
                const auto origin = "pickups[" + std::to_string(index) + "]";
                if (placement.definitionName.empty())
                {
                    result.itemReferences.emplace(origin + ".item", placement.stack.item);
                }
                else
                {
                    result.pickupReferences.emplace(
                        origin + ".definition", placement.definitionName);
                }
            }

            result.exit = levelExit(member(root, "exit", sourceName, "root"), sourceName);
            result.exitReferences.emplace("exit.definition", result.exit.definitionName);
            if (result.exit.requirement)
            {
                result.itemReferences.emplace(
                    "exit.requirement.item", result.exit.requirement->item);
            }
            return result;
        }
    }

    ExampleLevelData parseExampleLevelData(std::string_view text, std::string_view sourceName)
    {
        try
        {
            return levelData(
                expandObjectLegend(Json::parse(text.begin(), text.end()), sourceName), sourceName);
        }
        catch (const Json::parse_error& exception)
        {
            // JSON reports a one-based byte position, including one past the end at EOF.
            std::size_t line = 1;
            std::size_t column = 1;
            for (std::size_t index = 0; index < text.size() && index + 1 < exception.byte; ++index)
            {
                if (text[index] == '\n')
                {
                    ++line;
                    column = 1;
                }
                else
                {
                    ++column;
                }
            }
            throw std::invalid_argument(
                std::string(sourceName) + ": line " + std::to_string(line) + ", column " +
                std::to_string(column) + ": invalid JSON: " + exception.what());
        }
        catch (const Json::exception& exception)
        {
            throw std::invalid_argument(
                std::string(sourceName) + ": invalid JSON: " + exception.what());
        }
    }

    ExampleLevelData loadExampleLevelData(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        if (!file)
        {
            throw std::invalid_argument("Could not open level data '" + path.string() + "'");
        }
        const std::string contents{
            std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        return parseExampleLevelData(contents, path.string());
    }
}
