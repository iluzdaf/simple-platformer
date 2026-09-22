#include "pickup_catalog.hpp"
#include "content_diagnostics.hpp"
#include "content_json.hpp"
#include "content_validation.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <stdexcept>
#include "content/item_catalog.hpp"
#include <string_view>
#include <string>
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/world/pickup.hpp"

namespace simple_platformer
{
    void validatePickupDefinition(const PickupDefinition& definition, const ItemCatalog& items)
    {
        Pickup pickup;
        pickup.body.bounds.size = definition.bodySize;
        pickup.stack = composeItemStack(items, definition.stack);
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
                failJson({}, fieldPath("pickups", entry.first), error.what());
            }
        }
    }

    PickupCatalog parsePickupCatalog(
        std::string_view text,
        std::string_view sourceName,
        const ItemCatalog& items)
    {
        const auto root = parseContentRoot(text, sourceName);
        checkJsonFields(root, {"pickups"}, sourceName, "root");
        const auto& definitions = requiredJsonMember(root, "pickups", sourceName, "root");
        checkJsonObject(definitions, sourceName, "pickups");
        PickupCatalog catalog;
        for (const auto& entry : definitions.items())
        {
            const std::string path = fieldPath("pickups", entry.key());
            const auto& value = entry.value();
            checkJsonFields(value, {"item", "quantity", "bodySize", "sprite"}, sourceName, path);
            PickupDefinition definition;
            definition.stack = {
                readText(value, "item", sourceName, path),
                readInteger(value, "quantity", sourceName, path)};
            readOptionalVector(value, "bodySize", definition.bodySize, sourceName, path);
            if (value.contains("sprite"))
            {
                definition.sprite = jsonSprite(
                    requiredJsonMember(value, "sprite", sourceName, path),
                    sourceName,
                    fieldPath(path, "sprite"));
            }
            catalog.emplace(entry.key(), definition);
        }
        // Validation is shared with C++ built catalogues, so it names the pickup but not the file.
        try
        {
            validatePickupCatalog(catalog, items);
        }
        catch (const std::invalid_argument& error)
        {
            failJson(sourceName, {}, error.what());
        }
        return catalog;
    }

    PickupCatalog loadPickupCatalog(const std::filesystem::path& path, const ItemCatalog& items)
    {
        return parsePickupCatalog(loadContentText(path), path.string(), items);
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
        result.body.bounds.size = definition.bodySize;
        placeFeetAt(result.body.bounds, spawnFeet);
        result.stack = composeItemStack(items, definition.stack);
        result.sprite = definition.sprite;
        if (result.sprite)
        {
            result.sprite->textureId = textureId;
        }
        validatePickup(result);
        return result;
    }
}
