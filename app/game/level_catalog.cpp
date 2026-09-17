#include "level_catalog.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

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

    int positiveInteger(const Json& value, std::string_view sourceName, std::string_view path)
    {
        if (!value.is_number_integer())
        {
            fail(sourceName, path, "expected an integer");
        }

        int result = 0;
        try
        {
            result = value.get<int>();
        }
        catch (const Json::out_of_range&)
        {
            fail(sourceName, path, "integer is outside the supported range");
        }
        if (result <= 0)
        {
            fail(sourceName, path, "level number must be positive");
        }
        return result;
    }

    std::filesystem::path relativeFile(
        const Json& value,
        std::string_view sourceName,
        std::string_view path)
    {
        if (!value.is_string())
        {
            fail(sourceName, path, "expected a file name");
        }

        const std::filesystem::path file = value.get<std::string>();
        if (file.empty() || file.is_absolute())
        {
            fail(sourceName, path, "file must be a non-empty relative path");
        }
        for (const std::filesystem::path& part : file)
        {
            if (part == "..")
            {
                fail(sourceName, path, "file must stay inside the level directory");
            }
        }
        return file;
    }

    simple_platformer::LevelCatalog catalog(
        const Json& root,
        std::string_view sourceName,
        const std::filesystem::path& directory)
    {
        simple_platformer::LevelCatalog result;
        result.startLevel = positiveInteger(
            member(root, "startLevel", sourceName, "root"), sourceName, "startLevel");
        result.directory = directory;

        const Json& levels = member(root, "levels", sourceName, "root");
        if (!levels.is_array() || levels.empty())
        {
            fail(sourceName, "levels", "expected at least one level");
        }

        result.levels.reserve(levels.size());
        for (std::size_t index = 0; index < levels.size(); ++index)
        {
            const Json& value = levels[index];
            const std::string path = "levels[" + std::to_string(index) + "]";
            simple_platformer::LevelCatalogEntry entry;
            entry.number = positiveInteger(
                member(value, "number", sourceName, path), sourceName, path + ".number");
            entry.file =
                relativeFile(member(value, "file", sourceName, path), sourceName, path + ".file");

            const auto duplicateNumber = std::find_if(
                result.levels.begin(),
                result.levels.end(),
                [&entry](const simple_platformer::LevelCatalogEntry& existing)
                { return existing.number == entry.number; });
            if (duplicateNumber != result.levels.end())
            {
                fail(sourceName, path + ".number", "level number is already listed");
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
            fail(sourceName, "startLevel", "level is not listed in the catalog");
        }
        return result;
    }
}

namespace simple_platformer
{
    LevelCatalog parseLevelCatalog(
        std::string_view text,
        std::string_view sourceName,
        const std::filesystem::path& directory)
    {
        try
        {
            return catalog(Json::parse(text.begin(), text.end()), sourceName, directory);
        }
        catch (const Json::exception& exception)
        {
            throw std::invalid_argument(
                std::string(sourceName) + ": invalid JSON: " + exception.what());
        }
    }

    LevelCatalog loadLevelCatalog(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        if (!file)
        {
            throw std::invalid_argument("Could not open level catalog '" + path.string() + "'");
        }
        const std::string contents{
            std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        return parseLevelCatalog(contents, path.string(), path.parent_path());
    }

    LevelCatalog loadLevelCatalog()
    {
        return loadLevelCatalog(std::filesystem::path("assets/levels/levels.json"));
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
        return catalog.directory / found->file;
    }
}
