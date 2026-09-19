#include "level_catalog.hpp"
#include "content_diagnostics.hpp"
#include "content_json.hpp"
#include <initializer_list>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace
{
    using Json = nlohmann::json;
    using simple_platformer::checkJsonFields;
    using simple_platformer::jsonInteger;

    using simple_platformer::failJson;
    using simple_platformer::jsonText;
    using simple_platformer::requiredJsonMember;

    int jsonPositiveInteger(const Json& value, std::string_view sourceName, std::string_view path)
    {
        const int result = jsonInteger(value, sourceName, path);
        if (result <= 0)
        {
            failJson(sourceName, path, "level number must be positive");
        }
        return result;
    }

    std::filesystem::path jsonRelativeFile(
        const Json& value,
        std::string_view sourceName,
        std::string_view path)
    {
        const std::filesystem::path file = jsonText(value, sourceName, path);
        if (file.empty() || file.is_absolute())
        {
            failJson(sourceName, path, "file must be a non-empty relative path");
        }
        for (const std::filesystem::path& part : file)
        {
            if (part == "..")
            {
                failJson(sourceName, path, "file must stay inside the level directory");
            }
        }
        return file;
    }

    simple_platformer::LevelCatalog jsonLevelCatalog(
        const Json& root,
        std::string_view sourceName,
        const std::filesystem::path& levelDirectory)
    {
        checkJsonFields(root, {"startLevel", "levels"}, sourceName, "root");
        simple_platformer::LevelCatalog result;
        result.startLevel = jsonPositiveInteger(
            requiredJsonMember(root, "startLevel", sourceName, "root"), sourceName, "startLevel");
        result.levelDirectory = levelDirectory;

        const Json& levels = requiredJsonMember(root, "levels", sourceName, "root");
        if (!levels.is_array() || levels.empty())
        {
            failJson(sourceName, "levels", "expected at least one level");
        }

        result.levels.reserve(levels.size());
        for (std::size_t index = 0; index < levels.size(); ++index)
        {
            const Json& value = levels[index];
            const std::string path = "levels[" + std::to_string(index) + "]";
            checkJsonFields(value, {"number", "file"}, sourceName, path);
            simple_platformer::LevelCatalogEntry entry;
            entry.number = jsonPositiveInteger(
                requiredJsonMember(value, "number", sourceName, path),
                sourceName,
                path + ".number");
            entry.relativeFile = jsonRelativeFile(
                requiredJsonMember(value, "file", sourceName, path), sourceName, path + ".file");

            const auto duplicateNumber = std::find_if(
                result.levels.begin(),
                result.levels.end(),
                [&entry](const simple_platformer::LevelCatalogEntry& existing)
                { return existing.number == entry.number; });
            if (duplicateNumber != result.levels.end())
            {
                failJson(sourceName, path + ".number", "level number is already listed");
            }
            result.levels.push_back(std::move(entry));
        }

        const auto start = std::find_if(
            result.levels.begin(),
            result.levels.end(),
            [&result](const simple_platformer::LevelCatalogEntry& entry)
            { return entry.number == result.startLevel; });
        if (start == result.levels.end())
        {
            failJson(sourceName, "startLevel", "level is not listed in the catalog");
        }
        return result;
    }
}

namespace simple_platformer
{
    LevelCatalog parseLevelCatalog(
        std::string_view text,
        std::string_view sourceName,
        const std::filesystem::path& levelDirectory)
    {
        const auto root = parseContentRoot(text, sourceName);
        try
        {
            return jsonLevelCatalog(root, sourceName, levelDirectory);
        }
        // Raw member access in the catalog reader can still raise a nlohmann error of its own.
        catch (const Json::exception& exception)
        {
            throw std::invalid_argument(
                std::string(sourceName) + ": invalid JSON: " + exception.what());
        }
    }

    LevelCatalog loadLevelCatalog(const std::filesystem::path& path)
    {
        return parseLevelCatalog(loadContentText(path), path.string(), path.parent_path());
    }

    LevelCatalog loadLevelCatalog()
    {
        return loadLevelCatalog(std::filesystem::path("assets/levels.json"));
    }

    std::filesystem::path levelPath(const LevelCatalog& catalog, int levelNumber)
    {
        const auto found = std::find_if(
            catalog.levels.begin(),
            catalog.levels.end(),
            [levelNumber](const LevelCatalogEntry& entry) { return entry.number == levelNumber; });
        if (found == catalog.levels.end())
        {
            throw std::invalid_argument(
                "Level " + std::to_string(levelNumber) + " is not in the catalog");
        }
        return catalog.levelDirectory / found->relativeFile;
    }
}
