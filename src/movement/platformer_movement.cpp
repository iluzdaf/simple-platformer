#include "simple_platformer/movement/platformer_movement.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <glm/vec2.hpp>

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/physics/collision.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
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

        void validate(
            const PlatformerMovementConfig& config,
            const InputIntentions& intentions,
            float deltaTime)
        {
            requireTimeStep(deltaTime, "Platformer movement");
            validatePlatformerMovementConfig(config);

            if (!isFinite(intentions.direction))
            {
                throw std::invalid_argument("Input intentions must be finite");
            }
        }

        void updateTimers(
            PlatformerMovement& movement,
            const InputIntentions& intentions,
            float deltaTime)
        {
            if (intentions.jumpPressed)
            {
                movement.jumpBufferRemaining = movement.config.jumpBufferDuration;
            }
            else
            {
                movement.jumpBufferRemaining =
                    std::max(0.0F, movement.jumpBufferRemaining - deltaTime);
            }

            if (movement.grounded)
            {
                movement.coyoteRemaining = movement.config.coyoteDuration;
            }
            else
            {
                movement.coyoteRemaining = std::max(0.0F, movement.coyoteRemaining - deltaTime);
            }
        }

        void updateHorizontalVelocity(
            Body& body,
            const PlatformerMovement& movement,
            const InputIntentions& intentions,
            float deltaTime)
        {
            const float direction = std::clamp(intentions.direction.x, -1.0F, 1.0F);
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

        void startBufferedJump(Body& body, PlatformerMovement& movement, bool jumpPressed)
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

        // Letting go of jump while rising pulls the actor down harder, for a short hop.
        void applyJumpAwareGravity(
            Body& body,
            const PlatformerMovement& movement,
            const InputIntentions& intentions,
            float deltaTime)
        {
            const bool cuttingJump = body.velocity.y < 0.0F && !intentions.jumpHeld;
            const float gravity =
                cuttingJump ? movement.config.jumpReleaseGravity : movement.config.gravity;
            applyGravity(body, gravity, movement.config.maximumFallSpeed, deltaTime);
        }
    }

    void validatePlatformerMovementConfig(const PlatformerMovementConfig& config)
    {
        const bool invalidConfig =
            !std::isfinite(config.maximumSpeed) || config.maximumSpeed < 0.0F ||
            !std::isfinite(config.groundAcceleration) || config.groundAcceleration < 0.0F ||
            !std::isfinite(config.airAcceleration) || config.airAcceleration < 0.0F ||
            !std::isfinite(config.groundDeceleration) || config.groundDeceleration < 0.0F ||
            !std::isfinite(config.jumpSpeed) || config.jumpSpeed < 0.0F ||
            !std::isfinite(config.gravity) || config.gravity < 0.0F ||
            !std::isfinite(config.jumpReleaseGravity) || config.jumpReleaseGravity < 0.0F ||
            !std::isfinite(config.maximumFallSpeed) || config.maximumFallSpeed < 0.0F ||
            !std::isfinite(config.coyoteDuration) || config.coyoteDuration < 0.0F ||
            !std::isfinite(config.jumpBufferDuration) || config.jumpBufferDuration < 0.0F;
        if (invalidConfig)
        {
            throw std::invalid_argument("Platformer movement configuration cannot be negative");
        }
    }

    Facing facingFor(const InputIntentions& intentions, Facing current)
    {
        const float decisive =
            intentions.aimDirection.x != 0.0F ? intentions.aimDirection.x : intentions.direction.x;
        if (decisive < 0.0F)
        {
            return Facing::Left;
        }
        return decisive > 0.0F ? Facing::Right : current;
    }

    CollisionContacts updatePlatformerMovement(
        const TileMap& map,
        Body& body,
        PlatformerMovement& movement,
        const InputIntentions& intentions,
        float deltaTime)
    {
        validate(movement.config, intentions, deltaTime);
        updateTimers(movement, intentions, deltaTime);
        updateHorizontalVelocity(body, movement, intentions, deltaTime);
        startBufferedJump(body, movement, intentions.jumpPressed);
        applyJumpAwareGravity(body, movement, intentions, deltaTime);

        const CollisionContacts contacts = moveBody(map, body, deltaTime);
        movement.grounded = contacts.ground;
        return contacts;
    }
}
