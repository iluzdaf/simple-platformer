#pragma once

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/physics/collision.hpp"

namespace simple_platformer
{
    class TileMap;

    enum class Facing
    {
        Left,
        Right
    };

    struct PlatformerMovementConfig
    {
        float maximumSpeed = 100.0F;
        float groundAcceleration = 800.0F;
        float airAcceleration = 400.0F;
        float groundDeceleration = 1000.0F;
        float jumpSpeed = 240.0F;
        float gravity = DefaultGravity;
        float jumpReleaseGravity = 2.0F * DefaultGravity;
        float maximumFallSpeed = DefaultMaximumFallSpeed;
        float coyoteDuration = 0.1F;
        float jumpBufferDuration = 0.1F;
    };

    void validatePlatformerMovementConfig(const PlatformerMovementConfig& config);

    // The one rule for which way an actor faces: aim decides when it points left or right,
    // otherwise the way the actor is trying to move, otherwise it stays as it was.
    Facing facingFor(const InputIntentions& intentions, Facing current);

    struct PlatformerMovement
    {
        PlatformerMovementConfig config;
        bool grounded = false;
        float coyoteRemaining = 0.0F;
        float jumpBufferRemaining = 0.0F;
    };

    CollisionContacts updatePlatformerMovement(
        const TileMap& map,
        Body& body,
        PlatformerMovement& movement,
        const InputIntentions& intentions,
        float deltaTime);
}
