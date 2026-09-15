#pragma once

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"

namespace simple_platformer
{
    struct Body
    {
        // Collision rectangle in world pixels. Its size is independent of any sprite.
        Aabb bounds;
        // Movement in world pixels per second.
        glm::vec2 velocity = {0.0F, 0.0F};
    };
}
