#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <glm/vec2.hpp>

#include "item_catalog.hpp"
#include "simple_platformer/math/coordinates.hpp"

namespace simple_platformer
{
    // A position as the level file gives it: a map cell, or feet in world pixels. The file
    // never knows the tile size, so cells stay cells until level composition has the map.
    using LevelPosition = std::variant<GridPosition, glm::vec2>;

    struct PatrolPlacement
    {
        LevelPosition first;
        LevelPosition second;
    };

    struct ActorPlacement
    {
        std::string definitionName;
        LevelPosition spawn;
        std::optional<PatrolPlacement> patrol;
    };

    struct PickupPlacement
    {
        LevelPosition spawn;
        NamedItemStack stack;
        // Empty selects an inline item stack with the default pickup appearance.
        std::string definitionName = {};
    };

    struct ExitPlacement
    {
        std::string definitionName;
        LevelPosition spawn;
        std::optional<NamedItemStack> requirement;
        bool consumeItem = false;
        std::optional<int> nextLevel;
    };

    // Parsed authoring data; level composition turns this into a runtime GameLevel.
    struct LevelData
    {
        std::map<char, std::string> tileLegend = {{'.', "empty"}, {'#', "stone"}};
        std::vector<std::string> mapRows;
        LevelPosition playerSpawn;
        std::vector<ActorPlacement> actors;
        // Includes unused legend templates so catalogue references can all be checked.
        // Each key is a diagnostic JSON path; its value is the referenced definition name.
        std::map<std::string, std::string> actorReferences;
        std::map<std::string, std::string> pickupReferences;
        std::map<std::string, std::string> exitReferences;
        std::map<std::string, std::string> itemReferences;
        std::vector<PickupPlacement> pickups;
        ExitPlacement exit;
    };

    LevelData parseLevelData(std::string_view text, std::string_view sourceName);
    LevelData loadLevelData(const std::filesystem::path& path);
}
