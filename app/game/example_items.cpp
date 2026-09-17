#include "example_items.hpp"

#include <vector>

#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    std::vector<ItemDefinition> makeExampleItems(int textureId)
    {
        const Sprite coin{textureId, {{0.0F, 216.0F}, {16.0F, 16.0F}}, {16.0F, 16.0F}};
        const Sprite potion{textureId, {{16.0F, 216.0F}, {16.0F, 16.0F}}, {16.0F, 16.0F}};
        const Sprite key{textureId, {{32.0F, 216.0F}, {16.0F, 16.0F}}, {16.0F, 16.0F}};
        return {
            {Coin, "Coin", coin, 99, ItemEffect::None, 0},
            {HealthPotion, "Health potion", potion, 5, ItemEffect::Heal, 2},
            {Key, "Key", key, 1, ItemEffect::None, 0}};
    }
}
