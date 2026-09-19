#include "pickup_catalog.hpp"
#include "content_json.hpp"
#include "content_validation.hpp"
#include <nlohmann/json.hpp>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include "game/item_catalog.hpp"
#include <string_view>
#include <string>
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/world/pickup.hpp"

namespace simple_platformer
{
    void validatePickupDefinition(const PickupDefinition& definition, const ItemCatalog& items)
    {
        Pickup pickup;
        pickup.bounds.size = definition.bodySize;
        pickup.stack = resolveItemStack(items, definition.stack);
        pickup.sprite = definition.sprite;
        validatePickup(pickup);
        if (definition.sprite)
        {
            validateContentSprite(*definition.sprite);
        }
    }
    void validatePickupCatalog(const PickupCatalog& catalog, const ItemCatalog& items)
    {
        for (const auto& entry : catalog)
        {
            try
            {
                if (entry.first.empty())
                {
                    throw std::invalid_argument("pickup name cannot be empty");
                }
                validatePickupDefinition(entry.second, items);
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument("pickups." + entry.first + ": " + error.what());
            }
        }
    }
    PickupCatalog parsePickupCatalog(
        std::string_view text,
        std::string_view sourceName,
        const ItemCatalog& items)
    {
        try
        {
            const auto root = nlohmann::json::parse(text);
            checkJsonFields(root, {"pickups"});
            const auto& definitions = root.at("pickups");
            if (!definitions.is_object())
            {
                throw std::invalid_argument("pickups: expected an object");
            }
            PickupCatalog catalog;
            for (const auto& entry : definitions.items())
            {
                try
                {
                    const auto& value = entry.value();
                    checkJsonFields(value, {"item", "quantity", "bodySize", "sprite"});
                    PickupDefinition definition;
                    definition.stack = {
                        value.at("item").get<std::string>(), jsonInteger(value.at("quantity"))};
                    if (value.contains("bodySize"))
                    {
                        definition.bodySize = jsonVector(value.at("bodySize"));
                    }
                    if (value.contains("sprite"))
                    {
                        definition.sprite = jsonSprite(value.at("sprite"));
                    }
                    catalog.emplace(entry.key(), definition);
                }
                catch (const std::exception& error)
                {
                    throw std::invalid_argument("pickups." + entry.key() + ": " + error.what());
                }
            }
            validatePickupCatalog(catalog, items);
            return catalog;
        }
        catch (const std::exception& error)
        {
            throw std::invalid_argument(std::string(sourceName) + ": " + error.what());
        }
    }
    PickupCatalog loadPickupCatalog(const std::filesystem::path& path, const ItemCatalog& items)
    {
        return parsePickupCatalog(readContentFile(path), path.string(), items);
    }
    const PickupDefinition& pickupDefinition(const PickupCatalog& catalog, const std::string& name)
    {
        const auto found = catalog.find(name);
        if (found == catalog.end())
        {
            throw std::invalid_argument("unknown pickup definition '" + name + "'");
        }
        return found->second;
    }
    Pickup composePickup(
        const PickupDefinition& definition,
        const ItemCatalog& items,
        int textureId,
        glm::vec2 spawnFeet)
    {
        validatePickupDefinition(definition, items);
        Pickup result;
        result.bounds.size = definition.bodySize;
        placeFeetAt(result.bounds, spawnFeet);
        result.stack = resolveItemStack(items, definition.stack);
        result.sprite = definition.sprite;
        if (result.sprite)
        {
            result.sprite->textureId = textureId;
        }
        validatePickup(result);
        return result;
    }
}
