#pragma once

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/physics/collision.hpp"

namespace simple_platformer
{
    class TileMap;
    struct Body;

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
        float gravity = 800.0F;
        float jumpReleaseGravity = 1600.0F;
        float maximumFallSpeed = 600.0F;
        float coyoteTime = 0.1F;
        float jumpBufferTime = 0.1F;
    };

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
        Facing& facing,
        float deltaTime);
}
