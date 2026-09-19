#include "exit_catalog.hpp"
#include "content_diagnostics.hpp"
#include "content_json.hpp"
#include "content_validation.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/level_exit.hpp"

namespace simple_platformer
{
    void validateExitDefinition(const ExitDefinition& definition)
    {
        LevelExit exit;
        exit.bounds.size = definition.bodySize;
        validateLevelExit(exit);
        validateContentSprite(definition.sprite);
    }

    void validateExitCatalog(const ExitCatalog& catalog)
    {
        for (const auto& entry : catalog)
        {
            try
            {
                if (entry.first.empty())
                {
                    throw std::invalid_argument("exit definition name cannot be empty");
                }
                validateExitDefinition(entry.second);
            }
            catch (const std::invalid_argument& error)
            {
                failJson({}, fieldPath("exits", entry.first), error.what());
            }
        }
    }

    ExitCatalog parseExitCatalog(std::string_view text, std::string_view sourceName)
    {
        const auto root = parseContentRoot(text, sourceName);
        checkJsonFields(root, {"exits"}, sourceName, "root");
        const auto& definitions = requiredJsonMember(root, "exits", sourceName, "root");
        checkJsonObject(definitions, sourceName, "exits");
        ExitCatalog catalog;
        for (const auto& entry : definitions.items())
        {
            const std::string path = fieldPath("exits", entry.key());
            const auto& value = entry.value();
            checkJsonFields(value, {"bodySize", "sprite"}, sourceName, path);
            ExitDefinition definition;
            definition.bodySize = readVector(value, "bodySize", sourceName, path);
            definition.sprite = jsonSprite(
                requiredJsonMember(value, "sprite", sourceName, path),
                sourceName,
                fieldPath(path, "sprite"));
            catalog.emplace(entry.key(), definition);
        }
        // Validation is shared with C++ built catalogues, so it names the definition but not
        // the file.
        try
        {
            validateExitCatalog(catalog);
        }
        catch (const std::invalid_argument& error)
        {
            failJson(sourceName, {}, error.what());
        }
        return catalog;
    }

    ExitCatalog loadExitCatalog(const std::filesystem::path& path)
    {
        return parseExitCatalog(loadContentText(path), path.string());
    }

    const ExitDefinition& exitDefinition(const ExitCatalog& catalog, const std::string& name)
    {
        const auto found = catalog.find(name);
        if (found == catalog.end())
        {
            throw std::invalid_argument("unknown exit definition '" + name + "'");
        }
        return found->second;
    }

    LevelExit composeExit(const ExitDefinition& definition, int textureId, glm::vec2 spawnFeet)
    {
        validateExitDefinition(definition);
        LevelExit exit;
        exit.bounds.size = definition.bodySize;
        placeFeetAt(exit.bounds, spawnFeet);
        Sprite sprite = definition.sprite;
        sprite.textureId = textureId;
        exit.sprite = sprite;
        validateLevelExit(exit);
        return exit;
    }
}
