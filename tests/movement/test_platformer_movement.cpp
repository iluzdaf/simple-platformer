#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/physics/collision.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace
{
    using Catch::Matchers::WithinAbs;
    using simple_platformer::Body;
    using simple_platformer::Facing;
    using simple_platformer::InputIntentions;
    using simple_platformer::PlatformerMovement;
    using simple_platformer::TileDefinition;
    using simple_platformer::TileMap;

    constexpr int MapWidth = 20;
    constexpr int MapHeight = 8;
    constexpr float FloorTop = 112.0F;

    TileMap makeFloorMap()
    {
        std::vector<int> tiles(static_cast<std::size_t>(MapWidth * MapHeight), 0);
        const std::size_t floorStart =
            static_cast<std::size_t>(MapHeight - 1) * static_cast<std::size_t>(MapWidth);
        for (int column = 0; column < MapWidth; ++column)
        {
            tiles[floorStart + static_cast<std::size_t>(column)] = 1;
        }

        return {MapWidth, MapHeight, std::move(tiles), {{false, false, {}}, {true, true, {}}}};
    }

    PlatformerMovement makeMovement()
    {
        PlatformerMovement movement;
        movement.config.maximumSpeed = 100.0F;
        movement.config.groundAcceleration = 200.0F;
        movement.config.airAcceleration = 100.0F;
        movement.config.groundDeceleration = 50.0F;
        movement.config.jumpSpeed = 200.0F;
        movement.config.gravity = 100.0F;
        movement.config.jumpReleaseGravity = 300.0F;
        movement.config.maximumFallSpeed = 600.0F;
        movement.config.coyoteTime = 0.1F;
        movement.config.jumpBufferTime = 0.1F;
        return movement;
    }

    Body bodyOnFloor()
    {
        return {{{80.0F, FloorTop - 12.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    }

    void requireNear(float actual, float expected)
    {
        REQUIRE_THAT(actual, WithinAbs(expected, 0.0001F));
    }
}

TEST_CASE("Ground movement accelerates and decelerates", "[movement][platformer]")
{
    const TileMap map = makeFloorMap();
    Body body = bodyOnFloor();
    PlatformerMovement movement = makeMovement();
    movement.grounded = true;
    Facing facing = Facing::Left;

    InputIntentions intentions;
    intentions.direction.x = 1.0F;
    simple_platformer::updatePlatformerMovement(map, body, movement, intentions, facing, 0.1F);

    requireNear(body.velocity.x, 20.0F);
    requireNear(body.bounds.position.x, 82.0F);
    REQUIRE(facing == Facing::Right);
    REQUIRE(movement.grounded);

    intentions.direction.x = 0.0F;
    simple_platformer::updatePlatformerMovement(map, body, movement, intentions, facing, 0.1F);

    requireNear(body.velocity.x, 15.0F);
    REQUIRE(facing == Facing::Right);
}

TEST_CASE("Air movement uses its separate acceleration", "[movement][platformer]")
{
    const TileMap map = makeFloorMap();
    Body body{{{80.0F, 40.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    PlatformerMovement movement = makeMovement();
    Facing facing = Facing::Right;
    InputIntentions intentions;
    intentions.direction.x = -1.0F;

    simple_platformer::updatePlatformerMovement(map, body, movement, intentions, facing, 0.1F);

    requireNear(body.velocity.x, -10.0F);
    requireNear(body.velocity.y, 10.0F);
    REQUIRE(facing == Facing::Left);
    REQUIRE_FALSE(movement.grounded);

    intentions.direction.x = 0.0F;
    simple_platformer::updatePlatformerMovement(map, body, movement, intentions, facing, 0.1F);

    requireNear(body.velocity.x, -10.0F);
    REQUIRE(facing == Facing::Left);
}

TEST_CASE("Horizontal acceleration stops at maximum speed", "[movement][platformer]")
{
    const TileMap map = makeFloorMap();
    Body body = bodyOnFloor();
    PlatformerMovement movement = makeMovement();
    movement.grounded = true;
    Facing facing = Facing::Right;
    InputIntentions intentions;
    intentions.direction.x = 1.0F;

    for (int update = 0; update < 10; ++update)
    {
        simple_platformer::updatePlatformerMovement(map, body, movement, intentions, facing, 0.1F);
    }

    requireNear(body.velocity.x, movement.config.maximumSpeed);
}

TEST_CASE("Grounded actors can jump", "[movement][platformer]")
{
    const TileMap map = makeFloorMap();
    Body body = bodyOnFloor();
    PlatformerMovement movement = makeMovement();
    movement.grounded = true;
    Facing facing = Facing::Right;
    InputIntentions intentions;
    intentions.jumpPressed = true;
    intentions.jumpHeld = true;

    simple_platformer::updatePlatformerMovement(map, body, movement, intentions, facing, 0.1F);

    requireNear(body.velocity.y, -190.0F);
    REQUIRE(body.bounds.position.y < FloorTop - body.bounds.size.y);
    REQUIRE_FALSE(movement.grounded);
    REQUIRE(movement.coyoteRemaining == 0.0F);
    REQUIRE(movement.jumpBufferRemaining == 0.0F);
}

TEST_CASE("Grounded jumping does not require assistance timers", "[movement][platformer]")
{
    const TileMap map = makeFloorMap();
    Body body = bodyOnFloor();
    PlatformerMovement movement = makeMovement();
    movement.grounded = true;
    movement.config.coyoteTime = 0.0F;
    movement.config.jumpBufferTime = 0.0F;
    Facing facing = Facing::Right;
    InputIntentions intentions;
    intentions.jumpPressed = true;
    intentions.jumpHeld = true;

    simple_platformer::updatePlatformerMovement(map, body, movement, intentions, facing, 0.1F);

    REQUIRE(body.velocity.y < 0.0F);
    REQUIRE_FALSE(movement.grounded);
}

TEST_CASE("Coyote time permits a jump shortly after leaving ground", "[movement][platformer]")
{
    const TileMap map = makeFloorMap();
    Body body{{{80.0F, 40.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    PlatformerMovement movement = makeMovement();
    movement.coyoteRemaining = 0.05F;
    Facing facing = Facing::Right;
    InputIntentions intentions;
    intentions.jumpPressed = true;
    intentions.jumpHeld = true;

    simple_platformer::updatePlatformerMovement(map, body, movement, intentions, facing, 0.01F);

    requireNear(body.velocity.y, -199.0F);
}

TEST_CASE("Expired coyote time does not permit a jump", "[movement][platformer]")
{
    const TileMap map = makeFloorMap();
    Body body{{{80.0F, 40.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    PlatformerMovement movement = makeMovement();
    movement.coyoteRemaining = 0.005F;
    Facing facing = Facing::Right;
    InputIntentions intentions;
    intentions.jumpPressed = true;
    intentions.jumpHeld = true;

    simple_platformer::updatePlatformerMovement(map, body, movement, intentions, facing, 0.01F);

    requireNear(body.velocity.y, 1.0F);
    REQUIRE(movement.jumpBufferRemaining > 0.0F);
}

TEST_CASE("A buffered jump starts after landing", "[movement][platformer]")
{
    const TileMap map = makeFloorMap();
    Body body{{{80.0F, FloorTop - 13.0F}, {12.0F, 12.0F}}, {0.0F, 30.0F}};
    PlatformerMovement movement = makeMovement();
    Facing facing = Facing::Right;
    InputIntentions intentions;
    intentions.jumpPressed = true;
    intentions.jumpHeld = true;

    simple_platformer::updatePlatformerMovement(map, body, movement, intentions, facing, 0.1F);

    REQUIRE(movement.grounded);
    REQUIRE(movement.jumpBufferRemaining > 0.0F);

    intentions.jumpPressed = false;
    simple_platformer::updatePlatformerMovement(map, body, movement, intentions, facing, 0.01F);

    REQUIRE(body.velocity.y < 0.0F);
    REQUIRE_FALSE(movement.grounded);
    REQUIRE(movement.jumpBufferRemaining == 0.0F);
}

TEST_CASE("Releasing jump early produces a shorter jump", "[movement][platformer]")
{
    const TileMap map = makeFloorMap();
    Body heldBody{{{60.0F, 60.0F}, {12.0F, 12.0F}}, {0.0F, -100.0F}};
    Body releasedBody = heldBody;
    PlatformerMovement heldMovement = makeMovement();
    PlatformerMovement releasedMovement = heldMovement;
    Facing heldFacing = Facing::Right;
    Facing releasedFacing = Facing::Right;
    InputIntentions heldIntentions;
    heldIntentions.jumpHeld = true;
    InputIntentions releasedIntentions;

    simple_platformer::updatePlatformerMovement(
        map, heldBody, heldMovement, heldIntentions, heldFacing, 0.1F);
    simple_platformer::updatePlatformerMovement(
        map, releasedBody, releasedMovement, releasedIntentions, releasedFacing, 0.1F);

    requireNear(heldBody.velocity.y, -90.0F);
    requireNear(releasedBody.velocity.y, -70.0F);
    REQUIRE(releasedBody.bounds.position.y > heldBody.bounds.position.y);
}

TEST_CASE("Falling speed is limited by terminal velocity", "[movement][platformer]")
{
    const TileMap map = makeFloorMap();
    Body body{{{80.0F, 20.0F}, {12.0F, 12.0F}}, {0.0F, 590.0F}};
    PlatformerMovement movement = makeMovement();
    Facing facing = Facing::Right;

    simple_platformer::updatePlatformerMovement(map, body, movement, {}, facing, 0.01F);

    requireNear(body.velocity.y, 591.0F);

    simple_platformer::updatePlatformerMovement(map, body, movement, {}, facing, 0.1F);

    requireNear(body.velocity.y, 600.0F);
}

TEST_CASE("Tile contacts stop velocity and update grounded state", "[movement][platformer]")
{
    const TileMap map = TileMap::fromAscii(
        {"...#....", "...#....", "...#....", "...#....", "...#....", "########"});
    Body body{{{20.0F, 60.0F}, {12.0F, 12.0F}}, {200.0F, 100.0F}};
    PlatformerMovement movement = makeMovement();
    movement.config.airAcceleration = 0.0F;
    Facing facing = Facing::Right;
    InputIntentions intentions;
    intentions.direction.x = 1.0F;

    const simple_platformer::CollisionContacts contacts =
        simple_platformer::updatePlatformerMovement(map, body, movement, intentions, facing, 0.2F);

    REQUIRE(contacts.right);
    REQUIRE(contacts.ground);
    REQUIRE(body.velocity.x == 0.0F);
    REQUIRE(body.velocity.y == 0.0F);
    REQUIRE(movement.grounded);
}

TEST_CASE("Invalid movement configuration and time steps are rejected", "[movement][platformer]")
{
    const TileMap map = makeFloorMap();
    Body body = bodyOnFloor();
    PlatformerMovement movement = makeMovement();
    Facing facing = Facing::Right;

    REQUIRE_THROWS_AS(
        simple_platformer::updatePlatformerMovement(map, body, movement, {}, facing, 0.0F),
        std::invalid_argument);

    movement.config.maximumFallSpeed = -1.0F;
    REQUIRE_THROWS_AS(
        simple_platformer::updatePlatformerMovement(map, body, movement, {}, facing, 0.1F),
        std::invalid_argument);
}
