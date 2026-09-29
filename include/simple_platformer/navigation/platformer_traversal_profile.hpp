#pragma once

#include <optional>

#include <glm/vec2.hpp>

#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/movement/surface_climb.hpp"

namespace simple_platformer
{
    // Static movement capabilities and the fixed simulation step. This is not live
    // actor state; matching profiles share cached connections.
    struct PlatformerTraversalProfile
    {
        PlatformerTraversalProfile() = default;

        PlatformerTraversalProfile(
            glm::vec2 bodySize,
            PlatformerMovementConfig movementConfig,
            float seconds,
            std::optional<SurfaceClimbConfig> climbConfig = std::nullopt)
            : size(bodySize),
              movement(movementConfig),
              stepSeconds(seconds),
              climb(climbConfig)
        {
        }

        glm::vec2 size = {0.0F, 0.0F};
        PlatformerMovementConfig movement;
        float stepSeconds = 0.0F;
        std::optional<SurfaceClimbConfig> climb;
    };

    inline bool operator==(
        const PlatformerTraversalProfile& left,
        const PlatformerTraversalProfile& right)
    {
        if (left.size != right.size || left.movement != right.movement ||
            left.stepSeconds != right.stepSeconds ||
            left.climb.has_value() != right.climb.has_value())
        {
            return false;
        }
        return !left.climb.has_value() || left.climb->speed == right.climb->speed;
    }
}
