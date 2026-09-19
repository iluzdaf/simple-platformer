#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include "simple_platformer/inventory/item.hpp"

namespace simple_platformer
{
    struct NamedItemStack
    {
        // Authoring name; composition resolves this to the engine's numeric ItemId.
        std::string item;
        int quantity = 1;
    };
    struct ItemCatalog
    {
        // IDs are assigned when loading; reuse this catalogue for a game session.
        std::map<std::string, ItemDefinition> definitions;
    };
    void validateItemCatalog(const ItemCatalog& catalog);
    ItemCatalog parseItemCatalog(std::string_view text, std::string_view sourceName);
    ItemCatalog loadItemCatalog(const std::filesystem::path& path);
    const ItemDefinition& itemDefinition(const ItemCatalog& catalog, const std::string& name);
    ItemStack resolveItemStack(const ItemCatalog& catalog, const NamedItemStack& stack);
    std::vector<ItemDefinition> composeItems(const ItemCatalog& catalog, int textureId);
}
