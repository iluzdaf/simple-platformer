#pragma once

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/path_follower.hpp"

namespace tests
{
    // A box of this size standing on the bottom of a cell, where the game places actors.
    inline simple_platformer::Aabb boxStandingIn(
        simple_platformer::GridPosition cell,
        glm::vec2 size)
    {
        simple_platformer::Aabb box{{0.0F, 0.0F}, size};
        simple_platformer::placeFeetAt(box, simple_platformer::navigationFeet(cell));
        return box;
    }
}
