#pragma once
#include <optional>

#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    struct Pickup
    {
        Aabb bounds;
        ItemStack stack;
        // Without an override, rendering uses the item's inventory icon.
        std::optional<Sprite> sprite = std::nullopt;
    };

    class World;
    class WorldRequests;

    // Checks the pickup itself; World also checks that its item exists.
    void validatePickup(const Pickup& pickup);

    void updatePickups(const World& world, WorldRequests& requests);
}
