#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

namespace simple_platformer
{
    struct ExampleLevelEntry
    {
        int number = 0;
        std::filesystem::path file;
    };

    struct ExampleLevelCatalog
    {
        int startLevel = 0;
        std::filesystem::path directory;
        std::vector<ExampleLevelEntry> levels;
    };

    ExampleLevelCatalog parseExampleLevelCatalog(
        std::string_view text,
        std::string_view sourceName,
        const std::filesystem::path& directory = {});
    ExampleLevelCatalog loadExampleLevelCatalog(const std::filesystem::path& path);
    ExampleLevelCatalog loadExampleLevelCatalog();
    std::filesystem::path exampleLevelPath(const ExampleLevelCatalog& catalog, int levelNumber);
}
