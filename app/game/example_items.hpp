#pragma once

#include <vector>

#include "simple_platformer/inventory/item.hpp"

namespace simple_platformer
{
    constexpr ItemId Coin = 1;
    constexpr ItemId HealthPotion = 2;
    constexpr ItemId Key = 3;

    std::vector<ItemDefinition> makeExampleItems(int textureId);
}
