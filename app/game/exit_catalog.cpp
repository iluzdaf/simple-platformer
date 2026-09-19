#include "exit_catalog.hpp"
#include "content_json.hpp"
#include "content_validation.hpp"
#include <nlohmann/json.hpp>
#include <exception>
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
                throw std::invalid_argument("exits." + entry.first + ": " + error.what());
            }
        }
    }

    ExitCatalog parseExitCatalog(std::string_view text, std::string_view sourceName)
    {
        try
        {
            const auto root = nlohmann::json::parse(text);
            checkJsonFields(root, {"exits"});
            const auto& definitions = root.at("exits");
            if (!definitions.is_object())
            {
                throw std::invalid_argument("exits: expected an object");
            }
            ExitCatalog catalog;
            for (const auto& entry : definitions.items())
            {
                try
                {
                    const auto& value = entry.value();
                    checkJsonFields(value, {"bodySize", "sprite"});
                    ExitDefinition definition;
                    definition.bodySize = jsonVector(value.at("bodySize"));
                    definition.sprite = jsonSprite(value.at("sprite"));
                    catalog.emplace(entry.key(), definition);
                }
                catch (const std::exception& error)
                {
                    throw std::invalid_argument("exits." + entry.key() + ": " + error.what());
                }
            }
            validateExitCatalog(catalog);
            return catalog;
        }
        catch (const std::exception& error)
        {
            throw std::invalid_argument(std::string(sourceName) + ": " + error.what());
        }
    }

    ExitCatalog loadExitCatalog(const std::filesystem::path& path)
    {
        return parseExitCatalog(readContentFile(path), path.string());
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
