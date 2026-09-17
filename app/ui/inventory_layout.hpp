#pragma once

#include <cstddef>

namespace simple_platformer
{
    struct InventoryGridLayout
    {
        std::size_t columns = 0;
        std::size_t rows = 0;
    };

    // Inventory slots use up to three columns and add rows as capacity grows.
    InventoryGridLayout makeInventoryGridLayout(std::size_t slotCount);
}
