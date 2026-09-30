#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/render/actor_sprite.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"

namespace
{
    using simple_platformer::ActorSpritePlacement;
    using simple_platformer::ClimbSurface;
    using simple_platformer::Facing;

    constexpr glm::vec2 SpriteSize{32.0F, 30.0F};

    simple_platformer::Actor climber(ClimbSurface surface, Facing facing)
    {
        simple_platformer::Actor actor = tests::ActorBuilder::sized({12.0F, 12.0F})
                                             .at({16.0F, 32.0F})
                                             .walking()
                                             .climbing()
                                             .withSprite({0, {}, SpriteSize});
        tests::surfaceClimb(actor).surface = surface;
        actor.facing = facing;
        return actor;
    }

    // Turned as the renderer turns a sprite: clockwise on a screen whose y points down.
    glm::vec2 turn(glm::vec2 direction, float radians)
    {
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        return {
            direction.x * cosine - direction.y * sine, direction.x * sine + direction.y * cosine};
    }

    // Where the art's feet and head point once mirrored and turned. The art stands on a
    // floor facing right.
    glm::vec2 feetDirection(const ActorSpritePlacement& placement)
    {
        return turn({0.0F, 1.0F}, placement.rotationRadians);
    }

    glm::vec2 headDirection(const ActorSpritePlacement& placement)
    {
        return turn({placement.flipHorizontal ? -1.0F : 1.0F, 0.0F}, placement.rotationRadians);
    }

    bool pointsAlong(glm::vec2 direction, glm::vec2 expected)
    {
        return glm::distance(direction, expected) < 0.001F;
    }

    bool near(float value, float expected)
    {
        return std::abs(value - expected) < 0.001F;
    }

    // The turned rectangle, found from its corners, is what the placement says it covers.
    void requireVisibleIsTurnedDrawn(const ActorSpritePlacement& placement)
    {
        const glm::vec2 centre = simple_platformer::centerOf(placement.drawn);
        const glm::vec2 half = placement.drawn.size * 0.5F;
        glm::vec2 lowest = centre;
        glm::vec2 highest = centre;
        for (const glm::vec2 corner :
             {glm::vec2{-half.x, -half.y},
              glm::vec2{half.x, -half.y},
              glm::vec2{-half.x, half.y},
              glm::vec2{half.x, half.y}})
        {
            const glm::vec2 point = centre + turn(corner, placement.rotationRadians);
            lowest = glm::min(lowest, point);
            highest = glm::max(highest, point);
        }
        REQUIRE(near(placement.visible.position.x, lowest.x));
        REQUIRE(near(placement.visible.position.y, lowest.y));
        REQUIRE(near(placement.visible.position.x + placement.visible.size.x, highest.x));
        REQUIRE(near(placement.visible.position.y + placement.visible.size.y, highest.y));
    }
}

TEST_CASE("A sprite off every surface stands on the body's feet unturned", "[render][sprite]")
{
    simple_platformer::Actor actor = climber(ClimbSurface::None, Facing::Left);
    const ActorSpritePlacement placement = simple_platformer::placeActorSprite(actor);

    REQUIRE(placement.rotationRadians == 0.0F);
    REQUIRE(placement.flipHorizontal);
    const simple_platformer::Aabb expected =
        simple_platformer::spriteBounds(actor.body.bounds, tests::sprite(actor));
    REQUIRE(placement.drawn.position == expected.position);
    REQUIRE(placement.drawn.size == expected.size);
    REQUIRE(placement.visible.position == expected.position);
}

TEST_CASE("A climber's sprite stands its feet on the surface it holds", "[render][sprite][climb]")
{
    struct Case
    {
        ClimbSurface surface;
        glm::vec2 towardsSurface;
    };

    for (const Case& held :
         {Case{ClimbSurface::LeftWall, {-1.0F, 0.0F}},
          Case{ClimbSurface::RightWall, {1.0F, 0.0F}},
          Case{ClimbSurface::Ceiling, {0.0F, -1.0F}}})
    {
        const simple_platformer::Actor actor = climber(held.surface, Facing::Right);
        const ActorSpritePlacement placement = simple_platformer::placeActorSprite(actor);
        const simple_platformer::Aabb& body = actor.body.bounds;
        const glm::vec2 bodyCentre = simple_platformer::centerOf(body);
        const glm::vec2 visibleCentre = simple_platformer::centerOf(placement.visible);

        REQUIRE(pointsAlong(feetDirection(placement), held.towardsSurface));
        REQUIRE(placement.drawn.size == SpriteSize);
        requireVisibleIsTurnedDrawn(placement);
        switch (held.surface)
        {
        case ClimbSurface::LeftWall:
            REQUIRE(near(placement.visible.position.x, body.position.x));
            REQUIRE(near(visibleCentre.y, bodyCentre.y));
            break;
        case ClimbSurface::RightWall:
            REQUIRE(near(
                placement.visible.position.x + placement.visible.size.x,
                body.position.x + body.size.x));
            REQUIRE(near(visibleCentre.y, bodyCentre.y));
            break;
        case ClimbSurface::Ceiling:
            REQUIRE(near(placement.visible.position.y, body.position.y));
            REQUIRE(near(visibleCentre.x, bodyCentre.x));
            break;
        case ClimbSurface::None:
            break;
        }
    }
}

TEST_CASE("A climber's head leads the way it goes", "[render][sprite][climb]")
{
    // On a ceiling the head points the way the climber faces.
    REQUIRE(pointsAlong(
        headDirection(
            simple_platformer::placeActorSprite(climber(ClimbSurface::Ceiling, Facing::Right))),
        {1.0F, 0.0F}));
    REQUIRE(pointsAlong(
        headDirection(
            simple_platformer::placeActorSprite(climber(ClimbSurface::Ceiling, Facing::Left))),
        {-1.0F, 0.0F}));

    // On a wall it points the climb's wall heading.
    for (const ClimbSurface wall : {ClimbSurface::LeftWall, ClimbSurface::RightWall})
    {
        simple_platformer::Actor actor = climber(wall, Facing::Right);
        REQUIRE(
            pointsAlong(headDirection(simple_platformer::placeActorSprite(actor)), {0.0F, -1.0F}));

        tests::surfaceClimb(actor).wallHeading = simple_platformer::WallHeading::Down;
        REQUIRE(
            pointsAlong(headDirection(simple_platformer::placeActorSprite(actor)), {0.0F, 1.0F}));
    }
}

TEST_CASE("A centre-anchored climber's sprite turns about its body's centre", "[render][sprite]")
{
    simple_platformer::Actor actor = climber(ClimbSurface::LeftWall, Facing::Right);
    tests::sprite(actor).anchor = simple_platformer::SpriteAnchor::BodyCenter;
    const ActorSpritePlacement placement = simple_platformer::placeActorSprite(actor);

    const glm::vec2 bodyCentre = simple_platformer::centerOf(actor.body.bounds);
    REQUIRE(pointsAlong(simple_platformer::centerOf(placement.drawn), bodyCentre));
    REQUIRE(pointsAlong(simple_platformer::centerOf(placement.visible), bodyCentre));
}
