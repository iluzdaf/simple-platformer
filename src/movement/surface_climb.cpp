#include "simple_platformer/movement/surface_climb.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/physics/collision.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    namespace
    {
        ClimbSurface requestedSurface(
            ClimbSurface current,
            const CollisionContacts& contacts,
            const InputIntentions& intentions)
        {
            if (!intentions.climbRequested)
            {
                return ClimbSurface::None;
            }

            const float horizontal = std::abs(intentions.direction.x);
            const float vertical = std::abs(intentions.direction.y);
            if (contacts.ceiling && horizontal > vertical)
            {
                return ClimbSurface::Ceiling;
            }
            if (vertical > horizontal)
            {
                if (contacts.left)
                {
                    return ClimbSurface::LeftWall;
                }
                if (contacts.right)
                {
                    return ClimbSurface::RightWall;
                }
            }
            if (touchesSurface(current, contacts))
            {
                return current;
            }
            if (contacts.left)
            {
                return ClimbSurface::LeftWall;
            }
            if (contacts.right)
            {
                return ClimbSurface::RightWall;
            }
            return contacts.ceiling ? ClimbSurface::Ceiling : ClimbSurface::None;
        }
    }

    bool touchesSurface(ClimbSurface surface, const CollisionContacts& contacts)
    {
        switch (surface)
        {
        case ClimbSurface::LeftWall:
            return contacts.left;
        case ClimbSurface::RightWall:
            return contacts.right;
        case ClimbSurface::Ceiling:
            return contacts.ceiling;
        case ClimbSurface::None:
            return false;
        }
        return false;
    }

    void validateSurfaceClimbConfig(const SurfaceClimbConfig& config)
    {
        if (!std::isfinite(config.speed) || config.speed <= 0.0F)
        {
            throw std::invalid_argument("Surface climb speed must be finite and positive");
        }
    }

    WallHeading wallHeadingFor(
        ClimbSurface surface,
        const InputIntentions& intentions,
        WallHeading current)
    {
        if (surface != ClimbSurface::LeftWall && surface != ClimbSurface::RightWall)
        {
            return WallHeading::Up;
        }
        if (intentions.direction.y < 0.0F)
        {
            return WallHeading::Up;
        }
        return intentions.direction.y > 0.0F ? WallHeading::Down : current;
    }

    CollisionContacts updateSurfaceClimbMovement(
        const TileMap& map,
        Body& body,
        PlatformerMovement& movement,
        SurfaceClimb& climb,
        const InputIntentions& intentions,
        float deltaTime)
    {
        requireSeconds(deltaTime, "Surface climb time step");
        validateSurfaceClimbConfig(climb.config);
        validatePlatformerMovementConfig(movement.config);
        if (!isFinite(intentions.direction))
        {
            throw std::invalid_argument("Input intentions must be finite");
        }

        const ClimbSurface previous = climb.surface;
        climb.surface =
            requestedSurface(previous, touchingClimbableSurfaces(map, body.bounds), intentions);
        climb.wallHeading = wallHeadingFor(climb.surface, intentions, climb.wallHeading);
        if (climb.surface == ClimbSurface::None)
        {
            if (previous != ClimbSurface::None)
            {
                body.velocity = {0.0F, 0.0F};
            }
            return updatePlatformerMovement(map, body, movement, intentions, deltaTime);
        }

        body.velocity = {0.0F, 0.0F};
        if (climb.surface == ClimbSurface::Ceiling)
        {
            body.velocity.x = std::clamp(intentions.direction.x, -1.0F, 1.0F) * climb.config.speed;
        }
        else
        {
            body.velocity.y = std::clamp(intentions.direction.y, -1.0F, 1.0F) * climb.config.speed;
        }
        movement.coyoteRemaining = 0.0F;
        movement.jumpBufferRemaining = 0.0F;
        const CollisionContacts contacts = moveBody(map, body, deltaTime);
        const CollisionContacts surfaces = touchingSurfaces(map, body.bounds);
        movement.grounded = surfaces.ground;
        movement.blocked = contacts.left || contacts.right || contacts.ground || contacts.ceiling;
        if (!touchesSurface(climb.surface, touchingClimbableSurfaces(map, body.bounds)))
        {
            climb.surface = ClimbSurface::None;
            climb.wallHeading = WallHeading::Up;
            body.velocity = {0.0F, 0.0F};
        }
        return contacts;
    }
}
