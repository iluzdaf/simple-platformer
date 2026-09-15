#pragma once

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"

namespace simple_platformer
{
    struct Body
    {
        Aabb bounds;
        glm::vec2 velocity = {0.0F, 0.0F};
    };
}
