#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "simple_platformer/inventory/item.hpp"

namespace simple_platformer
{
    struct AddItemResult
    {
        int added = 0;
        int remaining = 0;
    };

    // Owns its slots so callers cannot bypass stack and quantity rules.
    class Inventory
    {
    public:
        explicit Inventory(std::size_t slotCount = 8);
        const std::vector<std::optional<ItemStack>>& slots() const;
        int count(ItemId item) const;
        AddItemResult add(const ItemDefinition& definition, int quantity);
        // Removal is all-or-nothing. Slot indexes are stable while the inventory is used.
        bool remove(ItemId item, int quantity);
        bool removeFromSlot(std::size_t slot, int quantity);

    private:
        std::vector<std::optional<ItemStack>> slotStorage;
    };
}
