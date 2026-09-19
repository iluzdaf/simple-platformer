#pragma once
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <glm/vec2.hpp>
#include "item_catalog.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/pickup.hpp"

namespace simple_platformer
{
    struct PickupDefinition
    {
        NamedItemStack stack;
        glm::vec2 bodySize = {16, 16};
        // Absent means use the inventory item's icon in the world too.
        std::optional<Sprite> sprite;
    };

    using PickupCatalog = std::map<std::string, PickupDefinition>;
    void validatePickupDefinition(const PickupDefinition& definition, const ItemCatalog& items);
    void validatePickupCatalog(const PickupCatalog& catalog, const ItemCatalog& items);
    PickupCatalog parsePickupCatalog(
        std::string_view text,
        std::string_view sourceName,
        const ItemCatalog& items);
    PickupCatalog loadPickupCatalog(const std::filesystem::path& path, const ItemCatalog& items);
    const PickupDefinition& pickupDefinition(const PickupCatalog& catalog, const std::string& name);
    Pickup composePickup(
        const PickupDefinition& definition,
        const ItemCatalog& items,
        int textureId,
        glm::vec2 spawnFeet);
}
