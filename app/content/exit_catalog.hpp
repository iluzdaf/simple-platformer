#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <glm/vec2.hpp>
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/level_exit.hpp"

namespace simple_platformer
{
    struct ExitDefinition
    {
        // Content declares it; composition rejects a size left at zero.
        glm::vec2 bodySize = {0.0F, 0.0F};
        Sprite sprite;
    };

    using ExitCatalog = std::map<std::string, ExitDefinition>;
    void validateExitDefinition(const ExitDefinition& definition);
    void validateExitCatalog(const ExitCatalog& catalog);
    ExitCatalog parseExitCatalog(std::string_view text, std::string_view sourceName);
    ExitCatalog loadExitCatalog(const std::filesystem::path& path);
    const ExitDefinition& exitDefinition(const ExitCatalog& catalog, const std::string& name);
    // Completion requirements and destination belong to the level placement.
    LevelExit composeExit(const ExitDefinition& definition, int textureId, glm::vec2 spawnFeet);
}
