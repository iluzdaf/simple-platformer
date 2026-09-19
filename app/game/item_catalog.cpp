#include "item_catalog.hpp"
#include "content_json.hpp"
#include "content_validation.hpp"
#include "simple_platformer/inventory/item.hpp"
#include <cstddef>
#include <nlohmann/json.hpp>
#include <exception>
#include <filesystem>
#include <set>
#include <limits>
#include <stdexcept>
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
                throw std::invalid_argument("items." + entry.first + ": " + error.what());
            }
        }
    }
    ItemCatalog parseItemCatalog(std::string_view text, std::string_view sourceName)
    {
        try
        {
            const auto root = nlohmann::json::parse(text);
            checkJsonFields(root, {"items"});
            const auto& definitions = root.at("items");
            if (!definitions.is_object())
            {
                throw std::invalid_argument("items: expected an object");
            }
            ItemCatalog catalog;
            if (definitions.size() > static_cast<std::size_t>(std::numeric_limits<ItemId>::max()))
            {
                throw std::invalid_argument("too many item definitions");
            }
            for (const auto& entry : definitions.items())
            {
                try
                {
                    const auto& value = entry.value();
                    checkJsonFields(
                        value, {"name", "icon", "maximumStack", "effect", "effectAmount"});
                    ItemDefinition item;
                    // JSON objects iterate by name. IDs are internal to this loaded catalogue.
                    item.id = static_cast<ItemId>(catalog.definitions.size() + 1);
                    item.name = value.at("name").get<std::string>();
                    item.icon = jsonSprite(value.at("icon"));
                    item.maximumStack = jsonInteger(value.at("maximumStack"));
                    const auto effect = value.value("effect", std::string("none"));
                    if (effect == "heal")
                    {
                        item.effect = ItemEffect::Heal;
                    }
                    else if (effect != "none")
                    {
                        throw std::invalid_argument("unknown effect '" + effect + "'");
                    }
                    if (value.contains("effectAmount"))
                    {
                        item.effectAmount = jsonInteger(value.at("effectAmount"));
                    }
                    catalog.definitions.emplace(entry.key(), item);
                }
                catch (const std::exception& error)
                {
                    throw std::invalid_argument("items." + entry.key() + ": " + error.what());
                }
            }
            validateItemCatalog(catalog);
            return catalog;
        }
        catch (const std::exception& error)
        {
            throw std::invalid_argument(std::string(sourceName) + ": " + error.what());
        }
    }
    ItemCatalog loadItemCatalog(const std::filesystem::path& path)
    {
        return parseItemCatalog(readContentFile(path), path.string());
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
    ItemStack resolveItemStack(const ItemCatalog& catalog, const NamedItemStack& stack)
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
