#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

namespace simple_platformer
{
    struct LevelCatalogEntry
    {
        int number = 0;
        std::filesystem::path relativeFile;
    };

    struct LevelCatalog
    {
        int startLevel = 0;
        std::filesystem::path levelDirectory;
        std::vector<LevelCatalogEntry> levels;
    };

    LevelCatalog parseLevelCatalog(
        std::string_view text,
        std::string_view sourceName,
        const std::filesystem::path& levelDirectory = {});
    LevelCatalog loadLevelCatalog(const std::filesystem::path& path);
    std::filesystem::path levelPath(const LevelCatalog& catalog, int levelNumber);
}
