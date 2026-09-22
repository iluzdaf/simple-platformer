#include "item_catalog.hpp"
#include "content_diagnostics.hpp"
#include "content_json.hpp"
#include "content_validation.hpp"
#include "simple_platformer/inventory/item.hpp"
#include <cstddef>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <set>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace simple_platformer
{
    void validateItemCatalog(const ItemCatalog& catalog)
    {
        std::set<ItemId> ids;
        for (const auto& entry : catalog.definitions)
        {
            try
            {
                if (entry.first.empty())
                {
                    throw std::invalid_argument("item name cannot be empty");
                }
                validateItemDefinition(entry.second);
                validateContentSprite(entry.second.icon);
                if (!ids.insert(entry.second.id).second)
                {
                    throw std::invalid_argument("repeated item ID");
                }
            }
            catch (const std::invalid_argument& error)
            {
                failJson({}, fieldPath("items", entry.first), error.what());
            }
        }
    }

    ItemCatalog parseItemCatalog(std::string_view text, std::string_view sourceName)
    {
        const auto root = parseContentRoot(text, sourceName);
        checkJsonFields(root, {"items"}, sourceName, "root");
        const auto& definitions = requiredJsonMember(root, "items", sourceName, "root");
        checkJsonObject(definitions, sourceName, "items");
        ItemCatalog catalog;
        if (definitions.size() > static_cast<std::size_t>(std::numeric_limits<ItemId>::max()))
        {
            failJson(sourceName, "items", "too many item definitions");
        }
        for (const auto& entry : definitions.items())
        {
            const std::string path = fieldPath("items", entry.key());
            const auto& value = entry.value();
            checkJsonFields(
                value,
                {"name", "icon", "maximumStack", "effect", "effectAmount"},
                sourceName,
                path);
            ItemDefinition item;
            // JSON objects iterate by name. IDs are internal to this loaded catalogue.
            item.id = static_cast<ItemId>(catalog.definitions.size() + 1);
            item.name = readText(value, "name", sourceName, path);
            item.icon = jsonSprite(
                requiredJsonMember(value, "icon", sourceName, path),
                sourceName,
                fieldPath(path, "icon"));
            item.maximumStack = readInteger(value, "maximumStack", sourceName, path);
            std::string effect = "none";
            readOptionalText(value, "effect", effect, sourceName, path);
            if (effect == "heal")
            {
                item.effect = ItemEffect::Heal;
            }
            else if (effect != "none")
            {
                failJson(
                    sourceName,
                    fieldPath(path, "effect"),
                    "unknown effect '" + effect + "'; expected heal or none");
            }
            readOptionalInteger(value, "effectAmount", item.effectAmount, sourceName, path);
            catalog.definitions.emplace(entry.key(), item);
        }
        validateInFile(sourceName, [&] { validateItemCatalog(catalog); });
        return catalog;
    }

    ItemCatalog loadItemCatalog(const std::filesystem::path& path)
    {
        return parseItemCatalog(loadContentText(path), path.string());
    }

    const ItemDefinition& itemDefinition(const ItemCatalog& catalog, const std::string& name)
    {
        const auto found = catalog.definitions.find(name);
        if (found == catalog.definitions.end())
        {
            throw std::invalid_argument("unknown item '" + name + "'");
        }
        return found->second;
    }

    ItemStack composeItemStack(const ItemCatalog& catalog, const NamedItemStack& stack)
    {
        if (stack.quantity <= 0)
        {
            throw std::invalid_argument("item quantity must be positive");
        }
        return {itemDefinition(catalog, stack.item).id, stack.quantity};
    }

    std::vector<ItemDefinition> composeItems(const ItemCatalog& catalog, int textureId)
    {
        validateItemCatalog(catalog);
        std::vector<ItemDefinition> result;
        for (const auto& entry : catalog.definitions)
        {
            auto item = entry.second;
            item.icon.textureId = textureId;
            result.push_back(item);
        }
        return result;
    }
}
