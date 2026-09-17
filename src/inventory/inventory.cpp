#include "simple_platformer/inventory/inventory.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

#include "simple_platformer/inventory/item.hpp"

namespace simple_platformer
{
    Inventory::Inventory(std::size_t slotCount) : slotStorage(slotCount)
    {
    }

    const std::vector<std::optional<ItemStack>>& Inventory::slots() const
    {
        return slotStorage;
    }

    int Inventory::count(ItemId item) const
    {
        int total = 0;
        for (const auto& slot : slotStorage)
        {
            if (slot.has_value() && slot->item == item)
            {
                total += slot->quantity;
            }
        }
        return total;
    }

    AddItemResult Inventory::add(const ItemDefinition& definition, int quantity)
    {
        validateItemDefinition(definition);
        if (quantity <= 0 || quantity > std::numeric_limits<int>::max() - count(definition.id))
        {
            throw std::invalid_argument("Item quantity must be positive and fit in an int");
        }
        int remaining = quantity;
        for (auto& slot : slotStorage)
        {
            if (slot.has_value() && slot->item == definition.id)
            {
                const int added =
                    std::min(remaining, std::max(0, definition.maximumStack - slot->quantity));
                slot->quantity += added;
                remaining -= added;
            }
        }
        for (auto& slot : slotStorage)
        {
            if (remaining > 0 && !slot.has_value())
            {
                const int added = std::min(remaining, definition.maximumStack);
                slot = ItemStack{definition.id, added};
                remaining -= added;
            }
        }
        return {quantity - remaining, remaining};
    }

    bool Inventory::remove(ItemId item, int quantity)
    {
        if (item <= 0 || quantity <= 0)
        {
            throw std::invalid_argument("Removal requires an item and positive quantity");
        }
        if (count(item) < quantity)
        {
            return false;
        }
        for (auto& slot : slotStorage)
        {
            if (slot.has_value() && slot->item == item)
            {
                const int removed = std::min(quantity, slot->quantity);
                slot->quantity -= removed;
                quantity -= removed;
                if (slot->quantity == 0)
                {
                    slot.reset();
                }
            }
        }
        return true;
    }

    bool Inventory::removeFromSlot(std::size_t slot, int quantity)
    {
        if (quantity <= 0)
        {
            throw std::invalid_argument("Removal quantity must be positive");
        }
        if (slot >= slotStorage.size())
        {
            return false;
        }
        auto& selected = slotStorage[slot];
        if (!selected.has_value() || selected->quantity < quantity)
        {
            return false;
        }
        selected->quantity -= quantity;
        if (selected->quantity == 0)
        {
            selected.reset();
        }
        return true;
    }
}
