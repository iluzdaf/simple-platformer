#include "simple_platformer/movement/platformer_movement.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <glm/vec2.hpp>

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/physics/collision.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace
{
    float moveTowards(float current, float target, float maximumChange)
    {
        if (current < target)
        {
            return std::min(current + maximumChange, target);
        }

        return std::max(current - maximumChange, target);
    }

    bool isFinite(float value)
    {
        return std::isfinite(value);
    }

    void validate(
        const simple_platformer::PlatformerMovementConfig& config,
        const simple_platformer::InputIntentions& intentions,
        float deltaTime)
    {
        if (!isFinite(deltaTime) || deltaTime <= 0.0F)
        {
            throw std::invalid_argument("Platformer movement requires a positive finite time step");
        }

        const bool invalidConfig =
            !isFinite(config.maximumSpeed) || config.maximumSpeed < 0.0F ||
            !isFinite(config.groundAcceleration) || config.groundAcceleration < 0.0F ||
            !isFinite(config.airAcceleration) || config.airAcceleration < 0.0F ||
            !isFinite(config.groundDeceleration) || config.groundDeceleration < 0.0F ||
            !isFinite(config.jumpSpeed) || config.jumpSpeed < 0.0F || !isFinite(config.gravity) ||
            config.gravity < 0.0F || !isFinite(config.jumpReleaseGravity) ||
            config.jumpReleaseGravity < 0.0F || !isFinite(config.maximumFallSpeed) ||
            config.maximumFallSpeed < 0.0F || !isFinite(config.coyoteTime) ||
            config.coyoteTime < 0.0F || !isFinite(config.jumpBufferTime) ||
            config.jumpBufferTime < 0.0F;
        if (invalidConfig)
        {
            throw std::invalid_argument("Platformer movement configuration cannot be negative");
        }

        if (!isFinite(intentions.direction.x) || !isFinite(intentions.direction.y))
        {
            throw std::invalid_argument("Input intentions must be finite");
        }
    }

    void updateTimers(
        simple_platformer::PlatformerMovement& movement,
        const simple_platformer::InputIntentions& intentions,
        float deltaTime)
    {
        if (intentions.jumpPressed)
        {
            movement.jumpBufferRemaining = movement.config.jumpBufferTime;
        }
        else
        {
            movement.jumpBufferRemaining = std::max(0.0F, movement.jumpBufferRemaining - deltaTime);
        }

        if (movement.grounded)
        {
            movement.coyoteRemaining = movement.config.coyoteTime;
        }
        else
        {
            movement.coyoteRemaining = std::max(0.0F, movement.coyoteRemaining - deltaTime);
        }
    }

    void updateHorizontalVelocity(
        simple_platformer::Body& body,
        const simple_platformer::PlatformerMovement& movement,
        const simple_platformer::InputIntentions& intentions,
        simple_platformer::Facing& facing,
        float deltaTime)
    {
        const float direction = std::clamp(intentions.direction.x, -1.0F, 1.0F);
        if (direction < 0.0F)
        {
            facing = simple_platformer::Facing::Left;
        }
        else if (direction > 0.0F)
        {
            facing = simple_platformer::Facing::Right;
        }

        if (direction == 0.0F && !movement.grounded)
        {
            return;
        }

        const float target = direction * movement.config.maximumSpeed;
        const float acceleration = direction == 0.0F   ? movement.config.groundDeceleration
                                   : movement.grounded ? movement.config.groundAcceleration
                                                       : movement.config.airAcceleration;
        body.velocity.x = moveTowards(body.velocity.x, target, acceleration * deltaTime);
    }

    void startBufferedJump(
        simple_platformer::Body& body,
        simple_platformer::PlatformerMovement& movement,
        bool jumpPressed)
    {
        const bool wantsToJump = jumpPressed || movement.jumpBufferRemaining > 0.0F;
        const bool canJump = movement.grounded || movement.coyoteRemaining > 0.0F;
        if (!wantsToJump || !canJump)
        {
            return;
        }

        body.velocity.y = -movement.config.jumpSpeed;
        movement.grounded = false;
        movement.coyoteRemaining = 0.0F;
        movement.jumpBufferRemaining = 0.0F;
    }

    void applyGravity(
        simple_platformer::Body& body,
        const simple_platformer::PlatformerMovement& movement,
        const simple_platformer::InputIntentions& intentions,
        float deltaTime)
    {
        const bool cuttingJump = body.velocity.y < 0.0F && !intentions.jumpHeld;
        const float gravity =
            cuttingJump ? movement.config.jumpReleaseGravity : movement.config.gravity;
        body.velocity.y =
            std::min(body.velocity.y + gravity * deltaTime, movement.config.maximumFallSpeed);
    }
}

namespace simple_platformer
{
    CollisionContacts updatePlatformerMovement(
        const TileMap& map,
        Body& body,
        PlatformerMovement& movement,
        const InputIntentions& intentions,
        Facing& facing,
        float deltaTime)
    {
        validate(movement.config, intentions, deltaTime);
        updateTimers(movement, intentions, deltaTime);
        updateHorizontalVelocity(body, movement, intentions, facing, deltaTime);
        startBufferedJump(body, movement, intentions.jumpPressed);
        applyGravity(body, movement, intentions, deltaTime);

        const CollisionContacts contacts =
            moveAndCollide(map, body.bounds, body.velocity * deltaTime);
        if (contacts.left || contacts.right)
        {
            body.velocity.x = 0.0F;
        }
        if (contacts.ground || contacts.ceiling)
        {
            body.velocity.y = 0.0F;
        }

        movement.grounded = contacts.ground;
        return contacts;
    }
}
