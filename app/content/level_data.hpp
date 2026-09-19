#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

#include "item_catalog.hpp"
#include "simple_platformer/npc/npc.hpp"

namespace simple_platformer
{
    struct ActorPlacement
    {
        std::string definitionName;
        glm::vec2 spawnFeet = {0.0F, 0.0F};
        std::optional<Patrol> patrol;
    };

    struct PickupPlacement
    {
        glm::vec2 spawnFeet = {0.0F, 0.0F};
        NamedItemStack stack;
        // Empty selects an inline item stack with the default pickup appearance.
        std::string definitionName = {};
    };

    struct ExitPlacement
    {
        std::string definitionName;
        glm::vec2 spawnFeet = {0.0F, 0.0F};
        std::optional<NamedItemStack> requirement;
        bool consumeItem = false;
        std::optional<int> nextLevel;
    };

    // Parsed authoring data; level composition turns this into a runtime GameLevel.
    struct LevelData
    {
        std::map<char, std::string> tileLegend = {{'.', "empty"}, {'#', "stone"}};
        std::vector<std::string> mapRows;
        glm::vec2 playerSpawnFeet = {0.0F, 0.0F};
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
