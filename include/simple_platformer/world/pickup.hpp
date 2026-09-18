#pragma once

#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/math/aabb.hpp"

namespace simple_platformer
{
    struct Pickup
    {
        Aabb bounds;
        ItemStack stack;
    };

    class World;
    class WorldRequests;

    void updatePickups(const World& world, WorldRequests& requests);
}
