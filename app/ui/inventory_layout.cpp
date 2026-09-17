#include "inventory_layout.hpp"

#include <algorithm>
#include <cstddef>

namespace simple_platformer
{
    InventoryGridLayout makeInventoryGridLayout(std::size_t slotCount)
    {
        constexpr std::size_t MaximumColumns = 3;
        const std::size_t columns = std::min(slotCount, MaximumColumns);
        if (columns == 0)
        {
            return {};
        }

        const std::size_t rows = (slotCount + columns - 1) / columns;
        return {columns, rows};
    }
}
