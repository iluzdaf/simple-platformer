#include "level_data.hpp"
#include "content_diagnostics.hpp"
#include "content_json.hpp"
#include "content_validation.hpp"

#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include "content/item_catalog.hpp"
#include "simple_platformer/math/coordinates.hpp"

namespace simple_platformer
{
    namespace
    {
        using Json = nlohmann::json;

        Cell jsonCell(const Json& value, std::string_view sourceName, std::string_view path)
        {
            checkJsonPair(value, "[column, row]", sourceName, path);
            return {
                jsonInteger(value[0], sourceName, indexPath(path, 0)),
                jsonInteger(value[1], sourceName, indexPath(path, 1))};
        }

        LevelPosition readPosition(
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
                return jsonCell(*cell, sourceName, fieldPath(path, cellKey));
            }
            return jsonVector(*feet, sourceName, fieldPath(path, feetKey));
        }

        PatrolPlacement jsonPatrol(
            const Json& value,
            std::string_view sourceName,
            std::string_view path)
        {
            checkJsonFields(
                value, {"firstCell", "firstFeet", "secondCell", "secondFeet"}, sourceName, path);
            return {
                readPosition(value, "firstCell", "firstFeet", sourceName, path),
                readPosition(value, "secondCell", "secondFeet", sourceName, path)};
        }

        ActorPlacement jsonActorSettings(
            const Json& value,
            std::string_view sourceName,
            const std::string& path)
        {
            ActorPlacement result;
            result.definitionName =
                readName(value, "definition", "actor definition name", sourceName, path);
            const auto found = value.find("patrol");
            if (found != value.end())
            {
                result.patrol = jsonPatrol(*found, sourceName, fieldPath(path, "patrol"));
            }
            return result;
        }

        PickupPlacement jsonPickupSettings(
            const Json& value,
            std::string_view sourceName,
            const std::string& path)
        {
            PickupPlacement result;
            if (value.contains("definition"))
            {
                if (value.contains("item") || value.contains("quantity") ||
                    value.contains("bodySize"))
                {
                    failJson(
                        sourceName,
                        path,
                        "use either a pickup definition or an inline item, quantity and bodySize");
                }
                result.definitionName =
                    readName(value, "definition", "pickup definition name", sourceName, path);
                return result;
            }
            result.stack = {
                readName(value, "item", "item name", sourceName, path),
                readInteger(value, "quantity", sourceName, path)};
            result.bodySize = readVector(value, "bodySize", sourceName, path);
            validatePickupSettings(result, path, sourceName);
            return result;
        }

        ExitPlacement jsonExitSettings(
            const Json& value,
            std::string_view sourceName,
            const std::string& path)
        {
            ExitPlacement result;
            result.definitionName = jsonText(
                requiredJsonMember(value, "definition", sourceName, path),
                sourceName,
                fieldPath(path, "definition"));

            if (const auto found = value.find("requirement"); found != value.end())
            {
                const Json& requirement = *found;
                const std::string requirementPath = fieldPath(path, "requirement");
                checkJsonFields(requirement, {"item", "quantity"}, sourceName, requirementPath);
                const int quantity = jsonInteger(
                    requiredJsonMember(requirement, "quantity", sourceName, requirementPath),
                    sourceName,
                    fieldPath(requirementPath, "quantity"));
                result.requirement = NamedItemStack{
                    readName(requirement, "item", "item name", sourceName, requirementPath),
                    quantity};
            }

            if (const auto found = value.find("consumeItem"); found != value.end())
            {
                result.consumeItem =
                    jsonBoolean(*found, sourceName, fieldPath(path, "consumeItem"));
            }
            if (const auto found = value.find("nextLevel"); found != value.end())
            {
                const int nextLevel = jsonInteger(*found, sourceName, fieldPath(path, "nextLevel"));
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
                const std::string path = indexPath("map", rowIndex);
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

        void collectActorReference(
            LevelData& level,
            const ActorPlacement& actor,
            const std::string& path)
        {
            level.actorReferences.emplace(fieldPath(path, "definition"), actor.definitionName);
        }

        void collectPickupReference(
            LevelData& level,
            const PickupPlacement& pickup,
            const std::string& path)
        {
            if (pickup.definitionName.empty())
            {
                level.itemReferences.emplace(fieldPath(path, "item"), pickup.stack.item);
            }
            else
            {
                level.pickupReferences.emplace(
                    fieldPath(path, "definition"), pickup.definitionName);
            }
        }

        void collectExitReferences(
            LevelData& level,
            const ExitPlacement& exit,
            const std::string& path)
        {
            level.exitReferences.emplace(fieldPath(path, "definition"), exit.definitionName);
            if (exit.requirement)
            {
                level.itemReferences.emplace(
                    fieldPath(path, "requirement.item"), exit.requirement->item);
            }
        }

        struct PlayerMarker
        {
        };

        // Templates contain parsed settings. Their spawn is assigned only when a map
        // cell copies the template into a placement; it is never read from template JSON.
        using ObjectTemplate =
            std::variant<PlayerMarker, ActorPlacement, PickupPlacement, ExitPlacement>;
        using ObjectLegend = std::map<char, ObjectTemplate>;

        ObjectTemplate jsonObjectTemplate(
            const Json& object,
            std::string_view sourceName,
            const std::string& path,
            LevelData& level)
        {
            const std::string type = readText(object, "type", sourceName, path);
            if (object.contains("spawnCell") || object.contains("spawnFeet"))
            {
                failJson(sourceName, path, "position comes from the map symbol");
            }
            if (type == "player")
            {
                checkJsonFields(object, {"type"}, sourceName, path);
                return PlayerMarker{};
            }
            if (type == "actor")
            {
                checkJsonFields(object, {"type", "definition", "patrol"}, sourceName, path);
                ActorPlacement actor = jsonActorSettings(object, sourceName, path);
                collectActorReference(level, actor, path);
                return actor;
            }
            if (type == "pickup")
            {
                checkJsonFields(
                    object,
                    {"type", "definition", "item", "quantity", "bodySize"},
                    sourceName,
                    path);
                PickupPlacement pickup = jsonPickupSettings(object, sourceName, path);
                collectPickupReference(level, pickup, path);
                return pickup;
            }
            if (type == "exit")
            {
                checkJsonFields(
                    object,
                    {"type", "definition", "requirement", "consumeItem", "nextLevel"},
                    sourceName,
                    path);
                ExitPlacement exit = jsonExitSettings(object, sourceName, path);
                collectExitReferences(level, exit, path);
                return exit;
            }
            failJson(
                sourceName,
                fieldPath(path, "type"),
                "unknown object type '" + type + "'; expected player, actor, pickup, or exit");
        }

        ObjectLegend jsonLegends(const Json& root, std::string_view sourceName, LevelData& level)
        {
            const Json& tiles = requiredJsonMember(root, "tileLegend", sourceName, "root");
            if (!tiles.is_object() || tiles.empty())
            {
                failJson(sourceName, "tileLegend", "expected a nonempty object");
            }
            const Json emptyLegend = Json::object();
            const Json& objects =
                root.contains("objectLegend") ? root.at("objectLegend") : emptyLegend;
            checkJsonObject(objects, sourceName, "objectLegend");
            checkLegendSymbols(tiles, objects, sourceName);
            for (const auto& entry : tiles.items())
            {
                level.tileLegend.emplace(
                    entry.key().front(), jsonText(entry.value(), sourceName, "tileLegend"));
            }

            ObjectLegend result;
            for (const auto& entry : objects.items())
            {
                const char symbol = entry.key().front();
                result.emplace(
                    symbol,
                    jsonObjectTemplate(
                        entry.value(), sourceName, fieldPath("objectLegend", entry.key()), level));
                // Object markers leave empty terrain at their cells.
                level.tileLegend.emplace(symbol, "empty");
            }
            return result;
        }

        struct PlacementOrigins
        {
            std::vector<PlacementOrigin> players;
            std::vector<PlacementOrigin> exits;
        };

        const Json& jsonPlacementArray(
            const Json& root,
            const char* key,
            std::string_view sourceName,
            const Json& emptyArray)
        {
            const Json& result = root.contains(key) ? root.at(key) : emptyArray;
            if (!result.is_array())
            {
                failJson(sourceName, key, "expected an array");
            }
            return result;
        }

        PlacementOrigins jsonExplicitPlacements(
            const Json& root,
            std::string_view sourceName,
            LevelData& level)
        {
            PlacementOrigins origins;
            origins.players = jsonPlayerOrigins(root);
            if (!origins.players.empty())
            {
                validateSinglePlacement(origins.players, "player", sourceName);
                level.playerSpawn =
                    readPosition(root, "playerSpawnCell", "playerSpawnFeet", sourceName, "root");
            }

            const Json emptyArray = Json::array();
            const Json& actors = jsonPlacementArray(root, "actors", sourceName, emptyArray);
            for (std::size_t index = 0; index < actors.size(); ++index)
            {
                const Json& object = actors[index];
                const std::string path = indexPath("actors", index);
                checkJsonFields(
                    object, {"definition", "spawnCell", "spawnFeet", "patrol"}, sourceName, path);
                ActorPlacement actor = jsonActorSettings(object, sourceName, path);
                actor.spawn = readPosition(object, "spawnCell", "spawnFeet", sourceName, path);
                collectActorReference(level, actor, path);
                level.actors.push_back(std::move(actor));
            }
            const Json& pickups = jsonPlacementArray(root, "pickups", sourceName, emptyArray);
            for (std::size_t index = 0; index < pickups.size(); ++index)
            {
                const Json& object = pickups[index];
                const std::string path = indexPath("pickups", index);
                checkJsonFields(
                    object,
                    {"definition", "item", "quantity", "bodySize", "spawnCell", "spawnFeet"},
                    sourceName,
                    path);
                PickupPlacement pickup = jsonPickupSettings(object, sourceName, path);
                pickup.spawn = readPosition(object, "spawnCell", "spawnFeet", sourceName, path);
                collectPickupReference(level, pickup, path);
                level.pickups.push_back(std::move(pickup));
            }
            if (root.contains("exit"))
            {
                const Json& object = root.at("exit");
                checkJsonFields(
                    object,
                    {"definition",
                     "spawnCell",
                     "spawnFeet",
                     "requirement",
                     "consumeItem",
                     "nextLevel"},
                    sourceName,
                    "exit");
                level.exit = jsonExitSettings(object, sourceName, "exit");
                level.exit.spawn =
                    readPosition(object, "spawnCell", "spawnFeet", sourceName, "exit");
                collectExitReferences(level, level.exit, "exit");
                origins.exits.push_back({"exit", std::nullopt});
            }
            return origins;
        }

        void placeMapObject(
            const ObjectTemplate& object,
            Cell cell,
            char symbol,
            std::string_view sourceName,
            LevelData& level,
            PlacementOrigins& origins)
        {
            const PlacementOrigin origin{
                indexPath(
                    indexPath("map", static_cast<std::size_t>(cell.y)),
                    static_cast<std::size_t>(cell.x)),
                symbol};
            if (std::holds_alternative<PlayerMarker>(object))
            {
                origins.players.push_back(origin);
                validateSinglePlacement(origins.players, "player", sourceName);
                level.playerSpawn = cell;
            }
            else if (const auto* actor = std::get_if<ActorPlacement>(&object))
            {
                level.actors.push_back(*actor);
                level.actors.back().spawn = cell;
            }
            else if (const auto* pickup = std::get_if<PickupPlacement>(&object))
            {
                level.pickups.push_back(*pickup);
                level.pickups.back().spawn = cell;
            }
            else if (const auto* exit = std::get_if<ExitPlacement>(&object))
            {
                origins.exits.push_back(origin);
                validateSinglePlacement(origins.exits, "exit", sourceName);
                level.exit = *exit;
                level.exit.spawn = cell;
            }
        }

        void placeMapObjects(
            const ObjectLegend& legend,
            std::string_view sourceName,
            LevelData& level,
            PlacementOrigins& origins)
        {
            for (std::size_t row = 0; row < level.mapRows.size(); ++row)
            {
                const std::string& cells = level.mapRows[row];
                for (std::size_t column = 0; column < cells.size(); ++column)
                {
                    const char symbol = cells[column];
                    const auto found = legend.find(symbol);
                    if (found != legend.end())
                    {
                        const Cell cell{static_cast<int>(column), static_cast<int>(row)};
                        placeMapObject(found->second, cell, symbol, sourceName, level, origins);
                    }
                }
            }
        }

        LevelData jsonLevelData(const Json& root, std::string_view sourceName)
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
            LevelData result;
            const ObjectLegend objects = jsonLegends(root, sourceName, result);
            result.mapRows = jsonMapRows(
                requiredJsonMember(root, "map", sourceName, "root"), sourceName, result.tileLegend);
            PlacementOrigins origins = jsonExplicitPlacements(root, sourceName, result);
            placeMapObjects(objects, sourceName, result, origins);
            validateSinglePlacement(origins.players, "player", sourceName);
            validateSinglePlacement(origins.exits, "exit", sourceName);
            return result;
        }
    }

    LevelData parseLevelData(std::string_view text, std::string_view sourceName)
    {
        const Json root = parseContentRoot(text, sourceName);
        try
        {
            return jsonLevelData(root, sourceName);
        }
        catch (const Json::exception& exception)
        {
            failJson(sourceName, {}, std::string("invalid JSON: ") + exception.what());
        }
    }

    LevelData loadLevelData(const std::filesystem::path& path)
    {
        return parseLevelData(loadContentText(path), path.string());
    }
}
