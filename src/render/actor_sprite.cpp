#include "simple_platformer/render/actor_sprite.hpp"

#include <stdexcept>

#include <glm/gtc/constants.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    namespace
    {
        ActorSpritePlacement turned(
            glm::vec2 visibleCentre,
            glm::vec2 size,
            float rotationRadians,
            bool flipHorizontal,
            bool quarterTurn)
        {
            const glm::vec2 visibleSize = quarterTurn ? glm::vec2{size.y, size.x} : size;
            return {
                boxCenteredOn(visibleCentre, size),
                rotationRadians,
                flipHorizontal,
                boxCenteredOn(visibleCentre, visibleSize)};
        }
    }

    ActorSpritePlacement placeActorSprite(const Actor& actor)
    {
        if (!actor.sprite.has_value())
        {
            throw std::invalid_argument("Placing an actor's sprite requires a sprite");
        }
        const Sprite& sprite = *actor.sprite;
        const Aabb& body = actor.body.bounds;
        const ClimbSurface surface =
            actor.surfaceClimb.has_value() ? actor.surfaceClimb->surface : ClimbSurface::None;
        const WallHeading wallHeading =
            actor.surfaceClimb.has_value() ? actor.surfaceClimb->wallHeading : WallHeading::Up;
        const bool facingLeft = actor.facing == Facing::Left;
        if (surface == ClimbSurface::None)
        {
            const Aabb bounds = spriteBounds(body, sprite);
            return {bounds, 0.0F, facingLeft, bounds};
        }

        const bool headingUp = wallHeading == WallHeading::Up;
        const bool fromFeet = sprite.anchor == SpriteAnchor::BodyFeet;
        const glm::vec2 centre = centerOf(body);
        // Half the sprite's height: how far its centre sits from the edge its feet are on.
        const float feetToCentre = sprite.size.y * 0.5F;
        // Unmirrored art heads right; a turn carries the head with it. A quarter turn
        // clockwise heads it down, one anticlockwise up, and a half turn left.
        switch (surface)
        {
        case ClimbSurface::Ceiling:
            return turned(
                fromFeet ? glm::vec2{centre.x, body.topLeft.y + feetToCentre} : centre,
                sprite.size,
                glm::pi<float>(),
                !facingLeft,
                false);
        case ClimbSurface::LeftWall:
            return turned(
                fromFeet ? glm::vec2{body.topLeft.x + feetToCentre, centre.y} : centre,
                sprite.size,
                glm::half_pi<float>(),
                headingUp,
                true);
        case ClimbSurface::RightWall:
            return turned(
                fromFeet ? glm::vec2{rightOf(body) - feetToCentre, centre.y} : centre,
                sprite.size,
                -glm::half_pi<float>(),
                !headingUp,
                true);
        case ClimbSurface::None:
            break;
        }
        throw std::invalid_argument("Climb surface is invalid");
    }
}
