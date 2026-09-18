#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/npc/npc.hpp"

namespace simple_platformer
{
    enum class ExampleActorType
    {
        Zombie,
        Bat,
        ZombieSoldier
    };

    struct ExampleActorPlacement
    {
        ExampleActorType type = ExampleActorType::Zombie;
        glm::vec2 spawnFeet = {0.0F, 0.0F};
        std::optional<Patrol> patrol;
    };

    struct ExamplePickupPlacement
    {
        glm::vec2 spawnFeet = {0.0F, 0.0F};
        ItemStack stack;
    };

    struct ExampleExitPlacement
    {
        glm::vec2 spawnFeet = {0.0F, 0.0F};
        std::optional<ItemStack> requirement;
        bool consumeItem = false;
        std::optional<int> nextLevel;
    };

    struct ExampleLevelData
    {
        std::map<char, std::string> tileLegend = {{'.', "empty"}, {'#', "stone"}};
        std::vector<std::string> mapRows;
        glm::vec2 playerSpawnFeet = {0.0F, 0.0F};
        std::vector<ExampleActorPlacement> actors;
        std::vector<ExamplePickupPlacement> pickups;
        ExampleExitPlacement exit;
    };

    ExampleLevelData parseExampleLevelData(std::string_view text, std::string_view sourceName);
    ExampleLevelData loadExampleLevelData(const std::filesystem::path& path);
}
