#include "level_data.hpp"
#include "content_diagnostics.hpp"
#include "content_json.hpp"
#include <initializer_list>
#include "content_validation.hpp"

#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "content/item_catalog.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"

namespace simple_platformer
{
    namespace
    {
        using Json = nlohmann::json;

        GridPosition jsonGridPosition(
            const Json& value,
            std::string_view sourceName,
            std::string_view path)
        {
            checkJsonPair(value, "[column, row]", sourceName, path);
            return {
                jsonInteger(value[0], sourceName, indexPath(path, 0)),
                jsonInteger(value[1], sourceName, indexPath(path, 1))};
        }

        glm::vec2 readFeetPosition(
            const Json& object,
            std::string_view cellKey,
            std::string_view feetKey,
            std::string_view sourceName,
            std::string_view path)
        {
            if (!object.is_object())
            {
                failJson(sourceName, path, "expected an object");
            }
            const auto cell = object.find(std::string(cellKey));
            const auto feet = object.find(std::string(feetKey));
            if ((cell == object.end()) == (feet == object.end()))
            {
                failJson(
                    sourceName,
                    path,
                    "supply exactly one of '" + std::string(cellKey) + "' or '" +
                        std::string(feetKey) + "'");
            }
            if (cell != object.end())
            {
                return navigationFeet(jsonGridPosition(
                    *cell, sourceName, std::string(path) + "." + std::string(cellKey)));
            }
            return jsonVector(*feet, sourceName, std::string(path) + "." + std::string(feetKey));
        }

        Patrol jsonPatrol(const Json& value, std::string_view sourceName, std::string_view path)
        {
            checkJsonFields(
                value, {"firstCell", "firstFeet", "secondCell", "secondFeet"}, sourceName, path);
            return {
                readFeetPosition(value, "firstCell", "firstFeet", sourceName, path),
                readFeetPosition(value, "secondCell", "secondFeet", sourceName, path),
                true};
        }

        ActorPlacement jsonActorPlacement(
            const Json& value,
            std::string_view sourceName,
            const std::string& path)
        {
            checkJsonFields(
                value, {"definition", "spawnCell", "spawnFeet", "patrol"}, sourceName, path);
            ActorPlacement result;
            result.definitionName =
                readName(value, "definition", "actor definition name", sourceName, path);
            result.spawnFeet = readFeetPosition(value, "spawnCell", "spawnFeet", sourceName, path);
            const auto found = value.find("patrol");
            if (found != value.end())
            {
                result.patrol = jsonPatrol(*found, sourceName, path + ".patrol");
            }
            return result;
        }

        PickupPlacement jsonPickupPlacement(
            const Json& value,
            std::string_view sourceName,
            const std::string& path)
        {
            checkJsonFields(
                value,
                {"definition", "item", "quantity", "spawnCell", "spawnFeet"},
                sourceName,
                path);
            if (value.contains("definition"))
            {
                if (value.contains("item") || value.contains("quantity"))
                {
                    failJson(
                        sourceName,
                        path,
                        "use either a pickup definition or an inline item and quantity");
                }
                PickupPlacement result;
                result.spawnFeet =
                    readFeetPosition(value, "spawnCell", "spawnFeet", sourceName, path);
                result.definitionName =
                    readName(value, "definition", "pickup definition name", sourceName, path);
                return result;
            }
            const int quantity = jsonInteger(
                requiredJsonMember(value, "quantity", sourceName, path),
                sourceName,
                path + ".quantity");
            const PickupPlacement result{
                readFeetPosition(value, "spawnCell", "spawnFeet", sourceName, path),
                {readName(value, "item", "item name", sourceName, path), quantity}};
            validatePickupSettings(result, path, sourceName);
            return result;
        }

        ExitPlacement jsonExitPlacement(
            const Json& value,
            std::string_view sourceName,
            const std::string& path = "exit")
        {
            checkJsonFields(
                value,
                {"definition", "spawnCell", "spawnFeet", "requirement", "consumeItem", "nextLevel"},
                sourceName,
                path);
            ExitPlacement result;
            result.definitionName = jsonText(
                requiredJsonMember(value, "definition", sourceName, path),
                sourceName,
                path + ".definition");
            result.spawnFeet = readFeetPosition(value, "spawnCell", "spawnFeet", sourceName, path);

            if (const auto found = value.find("requirement"); found != value.end())
            {
                const Json& requirement = *found;
                checkJsonFields(
                    requirement, {"item", "quantity"}, sourceName, path + ".requirement");
                const int quantity = jsonInteger(
                    requiredJsonMember(requirement, "quantity", sourceName, path + ".requirement"),
                    sourceName,
                    path + ".requirement.quantity");
                result.requirement = NamedItemStack{
                    readName(
                        requirement,
                        "item",
                        "item name",
                        sourceName,
                        fieldPath(path, "requirement")),
                    quantity};
            }

            if (const auto found = value.find("consumeItem"); found != value.end())
            {
                result.consumeItem = jsonBoolean(*found, sourceName, path + ".consumeItem");
            }
            if (const auto found = value.find("nextLevel"); found != value.end())
            {
                const int nextLevel = jsonInteger(*found, sourceName, path + ".nextLevel");
                result.nextLevel = nextLevel;
            }
            validateExitSettings(result, path, sourceName);
            return result;
        }

        std::vector<std::string> jsonMapRows(
            const Json& value,
            std::string_view sourceName,
            const std::map<char, std::string>& legend)
        {
            if (!value.is_array() || value.empty())
            {
                failJson(sourceName, "map", "expected at least one row");
            }

            std::vector<std::string> rows;
            rows.reserve(value.size());
            for (std::size_t rowIndex = 0; rowIndex < value.size(); ++rowIndex)
            {
                const std::string path = "map[" + std::to_string(rowIndex) + "]";
                rows.push_back(jsonText(value[rowIndex], sourceName, path));
            }
            validateMapRows(rows, legend, sourceName);
            return rows;
        }

        void checkLegendSymbols(const Json& tiles, const Json& objects, std::string_view sourceName)
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
            validateLegendSymbols(tileSymbols, objectSymbols, sourceName);
        }

        std::vector<PlacementOrigin> jsonPlayerOrigins(const Json& root)
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

        // Catalogue references found while validating the legend templates. Collecting them
        // here keeps the parse below from reading the same definitions out of JSON again.
        struct LegendReferences
        {
            std::map<std::string, std::string> actors;
            std::map<std::string, std::string> pickups;
            std::map<std::string, std::string> exits;
            std::map<std::string, std::string> items;
        };

        // Parses every legend template, including unused ones, so authoring mistakes in an
        // unplaced template are still reported. Records the catalogue names each one refers
        // to in references. The map cell is supplied later, when the symbols are scanned.
        void checkLegendTemplates(
            const Json& legend,
            std::string_view sourceName,
            LegendReferences& references)
        {
            for (const auto& entry : legend.items())
            {
                const std::string path = "objectLegend." + entry.key();
                const Json& object = entry.value();
                const std::string type = jsonText(
                    requiredJsonMember(object, "type", sourceName, path),
                    sourceName,
                    fieldPath(path, "type"));
                if (object.contains("spawnCell") || object.contains("spawnFeet"))
                {
                    failJson(sourceName, path, "position comes from the map symbol");
                }
                Json placement = object;
                placement.erase("type");
                placement["spawnCell"] = {0, 0};
                if (type == "pickup")
                {
                    const PickupPlacement parsed = jsonPickupPlacement(placement, sourceName, path);
                    if (parsed.definitionName.empty())
                    {
                        references.items.emplace(fieldPath(path, "item"), parsed.stack.item);
                    }
                    else
                    {
                        references.pickups.emplace(
                            fieldPath(path, "definition"), parsed.definitionName);
                    }
                }
                else if (type == "exit")
                {
                    const ExitPlacement parsed = jsonExitPlacement(placement, sourceName, path);
                    references.exits.emplace(fieldPath(path, "definition"), parsed.definitionName);
                    if (parsed.requirement)
                    {
                        references.items.emplace(
                            fieldPath(path, "requirement.item"), parsed.requirement->item);
                    }
                }
                else if (type == "actor")
                {
                    const ActorPlacement parsed = jsonActorPlacement(placement, sourceName, path);
                    references.actors.emplace(fieldPath(path, "definition"), parsed.definitionName);
                }
                else if (type == "player")
                {
                    checkJsonFields(object, {"type"}, sourceName, path);
                }
                else
                {
                    failJson(
                        sourceName,
                        fieldPath(path, "type"),
                        "unknown object type '" + type +
                            "'; expected player, actor, pickup, or exit");
                }
            }
        }

        // Turns each marked map cell into an explicit placement, so the shorthand and the
        // written-out arrays reach the parse below in the same shape.
        void expandMapSymbols(Json& root, const Json& legend, std::string_view sourceName)
        {
            for (const auto* key : {"actors", "pickups"})
            {
                if (!root.contains(key))
                {
                    root[key] = Json::array();
                }
                if (!root.at(key).is_array())
                {
                    failJson(sourceName, key, "expected an array");
                }
            }
            const Json& rows = requiredJsonMember(root, "map", sourceName, "root");
            if (!rows.is_array())
            {
                failJson(sourceName, "map", "expected an array");
            }
            auto players = jsonPlayerOrigins(root);
            std::vector<PlacementOrigin> exits;
            if (root.contains("exit"))
            {
                exits.push_back({"exit", std::nullopt});
            }
            for (std::size_t row = 0; row < rows.size(); ++row)
            {
                const std::string cells = jsonText(rows[row], sourceName, indexPath("map", row));
                for (std::size_t column = 0; column < cells.size(); ++column)
                {
                    const auto found = legend.find(std::string(1, cells[column]));
                    if (found == legend.end())
                    {
                        continue;
                    }
                    Json placement = *found;
                    const std::string type = readText(placement, "type");
                    placement.erase("type");
                    const Json spawnCell = {column, row};
                    placement["spawnCell"] = spawnCell;
                    const std::string location = indexPath(indexPath("map", row), column);
                    if (type == "player")
                    {
                        players.push_back({location, cells[column]});
                        validateSinglePlacement(players, "player", sourceName);
                        root["playerSpawnCell"] = spawnCell;
                    }
                    else if (type == "exit")
                    {
                        exits.push_back({location, cells[column]});
                        validateSinglePlacement(exits, "exit", sourceName);
                        root["exit"] = std::move(placement);
                    }
                    else
                    {
                        root[type == "pickup" ? "pickups" : "actors"].push_back(
                            std::move(placement));
                    }
                }
            }
        }

        // Expand the authoring shorthand into explicit placements, so both
        // ways of placing objects use the same parsing and validation below.
        Json expandObjectLegend(
            Json root,
            std::string_view sourceName,
            LegendReferences& references)
        {
            checkJsonFields(
                root,
                {"tileLegend",
                 "objectLegend",
                 "map",
                 "playerSpawnCell",
                 "playerSpawnFeet",
                 "actors",
                 "pickups",
                 "exit"},
                sourceName,
                "root");
            if (!root.contains("objectLegend"))
            {
                return root;
            }
            // Copied because the passes below add to root while this is being iterated.
            const Json legend = root.at("objectLegend");
            if (!legend.is_object())
            {
                failJson(sourceName, "objectLegend", "expected an object");
            }
            if (!root.contains("tileLegend"))
            {
                root["tileLegend"] = {{".", "empty"}, {"#", "stone"}};
            }
            const Json& tileLegend = requiredJsonMember(root, "tileLegend", sourceName, "root");
            if (!tileLegend.is_object() || tileLegend.empty())
            {
                failJson(sourceName, "tileLegend", "expected a nonempty object");
            }
            checkLegendSymbols(tileLegend, legend, sourceName);
            checkLegendTemplates(legend, sourceName, references);
            for (const auto& entry : legend.items())
            {
                // The marker creates an object, not a terrain tile.
                root["tileLegend"][entry.key()] = "empty";
            }
            expandMapSymbols(root, legend, sourceName);
            return root;
        }

        LevelData jsonLevelData(
            const Json& root,
            std::string_view sourceName,
            const LegendReferences& references)
        {
            LevelData result;
            if (root.contains("tileLegend"))
            {
                const auto& legend = root.at("tileLegend");
                if (!legend.is_object() || legend.empty())
                {
                    failJson(sourceName, "tileLegend", "expected a nonempty object");
                }
                result.tileLegend.clear();
                checkLegendSymbols(legend, Json::object(), sourceName);
                for (const auto& entry : legend.items())
                {
                    result.tileLegend.emplace(
                        entry.key().front(), jsonText(entry.value(), sourceName, "tileLegend"));
                }
            }
            result.mapRows = jsonMapRows(
                requiredJsonMember(root, "map", sourceName, "root"), sourceName, result.tileLegend);
            validateSinglePlacement(jsonPlayerOrigins(root), "player", sourceName);
            const std::vector<PlacementOrigin> exits =
                root.contains("exit") ? std::vector<PlacementOrigin>{{"exit", std::nullopt}}
                                      : std::vector<PlacementOrigin>{};
            validateSinglePlacement(exits, "exit", sourceName);
            result.playerSpawnFeet =
                readFeetPosition(root, "playerSpawnCell", "playerSpawnFeet", sourceName, "root");

            const Json& actors = requiredJsonMember(root, "actors", sourceName, "root");
            if (!actors.is_array())
            {
                failJson(sourceName, "actors", "expected an array");
            }
            result.actors.reserve(actors.size());
            // Legend templates were already parsed during expansion; reuse what they yielded.
            result.actorReferences = references.actors;
            result.pickupReferences = references.pickups;
            result.exitReferences = references.exits;
            result.itemReferences = references.items;
            for (std::size_t index = 0; index < actors.size(); ++index)
            {
                result.actors.push_back(jsonActorPlacement(
                    actors[index], sourceName, "actors[" + std::to_string(index) + "]"));
                result.actorReferences.emplace(
                    "actors[" + std::to_string(index) + "].definition",
                    result.actors.back().definitionName);
            }

            const Json& pickups = requiredJsonMember(root, "pickups", sourceName, "root");
            if (!pickups.is_array())
            {
                failJson(sourceName, "pickups", "expected an array");
            }
            result.pickups.reserve(pickups.size());
            for (std::size_t index = 0; index < pickups.size(); ++index)
            {
                result.pickups.push_back(jsonPickupPlacement(
                    pickups[index], sourceName, "pickups[" + std::to_string(index) + "]"));
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

            result.exit =
                jsonExitPlacement(requiredJsonMember(root, "exit", sourceName, "root"), sourceName);
            result.exitReferences.emplace("exit.definition", result.exit.definitionName);
            if (result.exit.requirement)
            {
                result.itemReferences.emplace(
                    "exit.requirement.item", result.exit.requirement->item);
            }
            return result;
        }
    }

    LevelData parseLevelData(std::string_view text, std::string_view sourceName)
    {
        auto root = parseContentRoot(text, sourceName);
        try
        {
            LegendReferences references;
            const Json expanded = expandObjectLegend(std::move(root), sourceName, references);
            return jsonLevelData(expanded, sourceName, references);
        }
        // Raw member access during expansion can still raise a nlohmann error of its own.
        catch (const Json::exception& exception)
        {
            throw std::invalid_argument(
                std::string(sourceName) + ": invalid JSON: " + exception.what());
        }
    }

    LevelData loadLevelData(const std::filesystem::path& path)
    {
        return parseLevelData(loadContentText(path), path.string());
    }
}
