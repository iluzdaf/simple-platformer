#pragma once

#include <cstddef>

#include "simple_platformer/actor/actor_id.hpp"

namespace simple_platformer
{
    class World;

    bool useItem(World& world, ActorId actor, std::size_t slot);
}
