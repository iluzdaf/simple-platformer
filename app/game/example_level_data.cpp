#include "example_level_data.hpp"

#include "example_items.hpp"

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "simple_platformer/inventory/item.hpp"
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

        ItemId itemId(const Json& value, std::string_view sourceName, std::string_view path)
        {
            const std::string name = text(value, sourceName, path);
            if (name == "coin")
            {
                return Coin;
            }
            if (name == "health_potion")
            {
                return HealthPotion;
            }
            if (name == "key")
            {
                return Key;
            }
            fail(sourceName, path, "unknown item '" + name + "'");
        }

        ExampleActorType actorType(
            const Json& value,
            std::string_view sourceName,
            std::string_view path)
        {
            const std::string name = text(value, sourceName, path);
            if (name == "zombie")
            {
                return ExampleActorType::Zombie;
            }
            if (name == "bat")
            {
                return ExampleActorType::Bat;
            }
            if (name == "zombie_soldier")
            {
                return ExampleActorType::ZombieSoldier;
            }
            fail(sourceName, path, "unknown actor type '" + name + "'");
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
            std::size_t index)
        {
            const std::string path = "actors[" + std::to_string(index) + "]";
            ExampleActorPlacement result;
            result.type =
                actorType(member(value, "type", sourceName, path), sourceName, path + ".type");
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
            std::size_t index)
        {
            const std::string path = "pickups[" + std::to_string(index) + "]";
            const int quantity = integer(
                member(value, "quantity", sourceName, path), sourceName, path + ".quantity");
            if (quantity <= 0)
            {
                fail(sourceName, path + ".quantity", "quantity must be positive");
            }
            return {
                feetPosition(value, "spawnCell", "spawnFeet", sourceName, path),
                {itemId(member(value, "item", sourceName, path), sourceName, path + ".item"),
                 quantity}};
        }

        ExampleExitPlacement levelExit(const Json& value, std::string_view sourceName)
        {
            constexpr std::string_view Path = "exit";
            ExampleExitPlacement result;
            result.spawnFeet = feetPosition(value, "spawnCell", "spawnFeet", sourceName, Path);

            if (const auto found = value.find("requirement"); found != value.end())
            {
                const Json& requirement = *found;
                const int quantity = integer(
                    member(requirement, "quantity", sourceName, "exit.requirement"),
                    sourceName,
                    "exit.requirement.quantity");
                if (quantity <= 0)
                {
                    fail(sourceName, "exit.requirement.quantity", "quantity must be positive");
                }
                result.requirement = ItemStack{
                    itemId(
                        member(requirement, "item", sourceName, "exit.requirement"),
                        sourceName,
                        "exit.requirement.item"),
                    quantity};
            }

            if (const auto found = value.find("consumeItem"); found != value.end())
            {
                result.consumeItem = boolean(*found, sourceName, "exit.consumeItem");
            }
            if (const auto found = value.find("nextLevel"); found != value.end())
            {
                const int nextLevel = integer(*found, sourceName, "exit.nextLevel");
                if (nextLevel <= 0)
                {
                    fail(sourceName, "exit.nextLevel", "level number must be positive");
                }
                result.nextLevel = nextLevel;
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
            std::size_t width = 0;
            for (std::size_t rowIndex = 0; rowIndex < value.size(); ++rowIndex)
            {
                const std::string path = "map[" + std::to_string(rowIndex) + "]";
                std::string row = text(value[rowIndex], sourceName, path);
                if (rowIndex == 0)
                {
                    width = row.size();
                    if (width == 0)
                    {
                        fail(sourceName, path, "row cannot be empty");
                    }
                }
                else if (row.size() != width)
                {
                    fail(sourceName, path, "every map row must have the same width");
                }
                for (const char symbol : row)
                {
                    if (legend.find(symbol) == legend.end())
                    {
                        fail(sourceName, path, "map symbol is not defined in tileLegend");
                    }
                }
                rows.push_back(std::move(row));
            }
            return rows;
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
                for (const auto& entry : legend.items())
                {
                    if (entry.key().size() != 1)
                    {
                        fail(sourceName, "tileLegend", "symbols must be one character");
                    }
                    result.tileLegend.emplace(
                        entry.key().front(), text(entry.value(), sourceName, "tileLegend"));
                }
            }
            result.mapRows =
                mapRows(member(root, "map", sourceName, "root"), sourceName, result.tileLegend);
            result.playerSpawnFeet =
                feetPosition(root, "playerSpawnCell", "playerSpawnFeet", sourceName, "root");

            const Json& actors = member(root, "actors", sourceName, "root");
            if (!actors.is_array())
            {
                fail(sourceName, "actors", "expected an array");
            }
            result.actors.reserve(actors.size());
            for (std::size_t index = 0; index < actors.size(); ++index)
            {
                result.actors.push_back(actor(actors[index], sourceName, index));
            }

            const Json& pickups = member(root, "pickups", sourceName, "root");
            if (!pickups.is_array())
            {
                fail(sourceName, "pickups", "expected an array");
            }
            result.pickups.reserve(pickups.size());
            for (std::size_t index = 0; index < pickups.size(); ++index)
            {
                result.pickups.push_back(pickup(pickups[index], sourceName, index));
            }

            result.exit = levelExit(member(root, "exit", sourceName, "root"), sourceName);
            return result;
        }
    }

    ExampleLevelData parseExampleLevelData(std::string_view text, std::string_view sourceName)
    {
        try
        {
            return levelData(Json::parse(text.begin(), text.end()), sourceName);
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
