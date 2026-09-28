#pragma once

#include <glm/vec2.hpp>

#include "simple_platformer/movement/platformer_movement.hpp"

namespace simple_platformer
{
    // The body size, movement settings, and fixed step used to simulate platformer
    // connections. This is not live actor state; matching profiles share results.
    struct PlatformerTraversalProfile
    {
        glm::vec2 size = {0.0F, 0.0F};
        PlatformerMovementConfig movement;
        float stepSeconds = 0.0F;
    };

    inline bool operator==(
        const PlatformerTraversalProfile& left,
        const PlatformerTraversalProfile& right)
    {
        return left.size == right.size && left.movement == right.movement &&
               left.stepSeconds == right.stepSeconds;
    }
}
