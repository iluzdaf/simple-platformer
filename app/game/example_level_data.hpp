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
    struct ExampleActorPlacement
    {
        std::string definitionName;
        glm::vec2 spawnFeet = {0.0F, 0.0F};
        std::optional<Patrol> patrol;
    };

    struct ExamplePickupPlacement
    {
        glm::vec2 spawnFeet = {0.0F, 0.0F};
        NamedItemStack stack;
        // Empty selects an inline item stack with the default pickup appearance.
        std::string definitionName = {};
    };

    struct ExampleExitPlacement
    {
        std::string definitionName;
        glm::vec2 spawnFeet = {0.0F, 0.0F};
        std::optional<NamedItemStack> requirement;
        bool consumeItem = false;
        std::optional<int> nextLevel;
    };

    struct ExampleLevelData
    {
        std::map<char, std::string> tileLegend = {{'.', "empty"}, {'#', "stone"}};
        std::vector<std::string> mapRows;
        glm::vec2 playerSpawnFeet = {0.0F, 0.0F};
        std::vector<ExampleActorPlacement> actors;
        // Includes unused legend templates so catalogue references can all be checked.
        std::map<std::string, std::string> actorReferences;
        std::map<std::string, std::string> pickupReferences;
        std::map<std::string, std::string> exitReferences;
        std::map<std::string, std::string> itemReferences;
        std::vector<ExamplePickupPlacement> pickups;
        ExampleExitPlacement exit;
    };

    ExampleLevelData parseExampleLevelData(std::string_view text, std::string_view sourceName);
    ExampleLevelData loadExampleLevelData(const std::filesystem::path& path);
}
