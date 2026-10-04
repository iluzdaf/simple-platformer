#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include <limits>
#include <stdexcept>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/require_near.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    using simple_platformer::Aabb;
    using simple_platformer::Body;
    using simple_platformer::CollisionContacts;
    using simple_platformer::TileMap;

    void requireNoContacts(const CollisionContacts& contacts)
    {
        REQUIRE_FALSE(contacts.left);
        REQUIRE_FALSE(contacts.right);
        REQUIRE_FALSE(contacts.ground);
        REQUIRE_FALSE(contacts.ceiling);
    }
}

// Movement and tile collision

TEST_CASE("A body moves by its velocity over the step", "[physics][body]")
{
    const TileMap map = tests::TileMapBuilder({"....", "....", "...."});
    Body body;
    body.bounds = {{8.0F, 8.0F}, {8.0F, 8.0F}};
    body.velocity = {120.0F, 90.0F};

    const CollisionContacts contacts = simple_platformer::moveBody(map, body, 0.1F);

    REQUIRE_NEAR(body.bounds.topLeft.x, 20.0F);
    REQUIRE_NEAR(body.bounds.topLeft.y, 17.0F);
    REQUIRE_NEAR(body.velocity.x, 120.0F);
    REQUIRE_NEAR(body.velocity.y, 90.0F);
    REQUIRE_FALSE(contacts.ground);
}

TEST_CASE("A body moves freely through empty tiles", "[physics][body][collision]")
{
    const TileMap map = tests::TileMapBuilder({"....", "....", "...."});
    Body body;
    body.bounds = {{8.0F, 8.0F}, {8.0F, 8.0F}};

    body.velocity = {12.0F, 9.0F};

    const CollisionContacts contacts = simple_platformer::moveBody(map, body, 1.0F);

    REQUIRE_NEAR(body.bounds.topLeft.x, 20.0F);
    REQUIRE_NEAR(body.bounds.topLeft.y, 17.0F);
    requireNoContacts(contacts);
}

TEST_CASE(
    "A body stops falling when it lands and stops sideways when it hits a wall",
    "[physics][body]")
{
    const TileMap map = tests::TileMapBuilder({"...#", "...#", "####"});

    SECTION("landing keeps the sideways speed")
    {
        Body body;
        body.bounds = {{8.0F, 16.0F}, {8.0F, 8.0F}};
        body.velocity = {20.0F, 200.0F};

        const CollisionContacts contacts = simple_platformer::moveBody(map, body, 0.1F);

        REQUIRE(contacts.ground);
        REQUIRE_NEAR(body.bounds.topLeft.y, 24.0F);
        REQUIRE_NEAR(body.velocity.y, 0.0F);
        REQUIRE_NEAR(body.velocity.x, 20.0F);
    }

    SECTION("hitting a wall keeps the vertical speed")
    {
        Body body;
        body.bounds = {{16.0F, 4.0F}, {8.0F, 8.0F}};
        body.velocity = {300.0F, -20.0F};

        const CollisionContacts contacts = simple_platformer::moveBody(map, body, 0.1F);

        REQUIRE(contacts.right);
        REQUIRE_NEAR(body.bounds.topLeft.x, 40.0F);
        REQUIRE_NEAR(body.velocity.x, 0.0F);
        REQUIRE_NEAR(body.velocity.y, -20.0F);
    }
}

TEST_CASE("Horizontal movement stops on either side of a solid tile", "[physics][body][collision]")
{
    const TileMap map = tests::TileMapBuilder({"..#...", "..#..."});

    SECTION("moving right")
    {
        Body body;
        body.bounds = {{8.0F, 4.0F}, {8.0F, 20.0F}};
        body.velocity = {30.0F, 0.0F};
        const CollisionContacts contacts = simple_platformer::moveBody(map, body, 1.0F);

        REQUIRE_NEAR(body.bounds.topLeft.x, 24.0F);
        REQUIRE_NEAR(body.bounds.topLeft.y, 4.0F);
        REQUIRE(contacts.right);
    }

    SECTION("moving left")
    {
        Body body;
        body.bounds = {{56.0F, 4.0F}, {8.0F, 20.0F}};
        body.velocity = {-30.0F, 0.0F};
        const CollisionContacts contacts = simple_platformer::moveBody(map, body, 1.0F);

        REQUIRE_NEAR(body.bounds.topLeft.x, 48.0F);
        REQUIRE_NEAR(body.bounds.topLeft.y, 4.0F);
        REQUIRE(contacts.left);
    }
}

TEST_CASE("Vertical movement stops on floors and ceilings", "[physics][body][collision]")
{
    SECTION("falling onto a floor")
    {
        const TileMap map = tests::TileMapBuilder({"....", "....", "####", "...."});
        Body body;
        body.bounds = {{20.0F, 4.0F}, {12.0F, 12.0F}};
        body.velocity = {0.0F, 30.0F};
        const CollisionContacts contacts = simple_platformer::moveBody(map, body, 1.0F);

        REQUIRE_NEAR(body.bounds.topLeft.x, 20.0F);
        REQUIRE_NEAR(body.bounds.topLeft.y, 20.0F);
        REQUIRE(contacts.ground);
    }

    SECTION("jumping into a ceiling")
    {
        const TileMap map = tests::TileMapBuilder({".##.", "....", "...."});
        Body body;
        body.bounds = {{20.0F, 24.0F}, {12.0F, 8.0F}};
        body.velocity = {0.0F, -20.0F};
        const CollisionContacts contacts = simple_platformer::moveBody(map, body, 1.0F);

        REQUIRE_NEAR(body.bounds.topLeft.x, 20.0F);
        REQUIRE_NEAR(body.bounds.topLeft.y, 16.0F);
        REQUIRE(contacts.ceiling);
    }
}

TEST_CASE("Fast movement cannot pass through a solid tile", "[physics][body][collision]")
{
    const TileMap map = tests::TileMapBuilder({"...#..", "...#.."});
    Body body;
    body.bounds = {{4.0F, 4.0F}, {8.0F, 20.0F}};

    body.velocity = {80.0F, 0.0F};

    const CollisionContacts contacts = simple_platformer::moveBody(map, body, 1.0F);

    REQUIRE_NEAR(body.bounds.topLeft.x, 40.0F);
    REQUIRE_NEAR(body.bounds.topLeft.y, 4.0F);
    REQUIRE(contacts.right);
}

TEST_CASE(
    "Collision resolves horizontal movement before vertical movement at a corner",
    "[physics][body][collision]")
{
    const TileMap map = tests::TileMapBuilder({"...", ".#.", "..."});
    Body body;
    body.bounds = {{0.0F, 0.0F}, {8.0F, 8.0F}};

    body.velocity = {16.0F, 16.0F};

    const CollisionContacts contacts = simple_platformer::moveBody(map, body, 1.0F);

    REQUIRE_NEAR(body.bounds.topLeft.x, 16.0F);
    REQUIRE_NEAR(body.bounds.topLeft.y, 8.0F);
    REQUIRE(contacts.ground);
    REQUIRE_FALSE(contacts.right);
}

TEST_CASE("Collision supports bodies larger than one tile", "[physics][body][collision]")
{
    const TileMap map = tests::TileMapBuilder({".....", ".....", ".....", "#####"});
    Body body;
    body.bounds = {{18.0F, 2.0F}, {28.0F, 30.0F}};

    body.velocity = {0.0F, 100.0F};

    const CollisionContacts contacts = simple_platformer::moveBody(map, body, 1.0F);

    REQUIRE_NEAR(body.bounds.topLeft.x, 18.0F);
    REQUIRE_NEAR(body.bounds.topLeft.y, 18.0F);
    REQUIRE(contacts.ground);
}

// Gravity

TEST_CASE("Gravity accelerates a falling body up to its maximum fall speed", "[physics][body]")
{
    Body body;
    body.bounds = {{0.0F, 0.0F}, {8.0F, 8.0F}};

    simple_platformer::applyGravity(body, 100.0F, 30.0F, 0.1F);
    REQUIRE_NEAR(body.velocity.y, 10.0F);

    simple_platformer::applyGravity(body, 100.0F, 30.0F, 0.1F);
    REQUIRE_NEAR(body.velocity.y, 20.0F);

    // The next step would reach 30 exactly; the one after clamps.
    simple_platformer::applyGravity(body, 100.0F, 30.0F, 0.1F);
    simple_platformer::applyGravity(body, 100.0F, 30.0F, 0.1F);
    REQUIRE_NEAR(body.velocity.y, 30.0F);
    REQUIRE_NEAR(body.velocity.x, 0.0F);
}

TEST_CASE("Gravity slows a rising body without touching its horizontal speed", "[physics][body]")
{
    Body body;
    body.bounds = {{0.0F, 0.0F}, {8.0F, 8.0F}};
    body.velocity = {5.0F, -50.0F};

    simple_platformer::applyGravity(body, 100.0F, 30.0F, 0.1F);

    REQUIRE_NEAR(body.velocity.x, 5.0F);
    REQUIRE_NEAR(body.velocity.y, -40.0F);
}

// Tile edges and map boundaries

TEST_CASE("The left, right and bottom map edges block movement", "[physics][body][collision]")
{
    const TileMap map = tests::TileMapBuilder({"....", "....", "...."});

    SECTION("left edge")
    {
        Body body;
        body.bounds = {{4.0F, 4.0F}, {8.0F, 8.0F}};
        body.velocity = {-20.0F, 0.0F};
        const CollisionContacts contacts = simple_platformer::moveBody(map, body, 1.0F);
        REQUIRE_NEAR(body.bounds.topLeft.x, 0.0F);
        REQUIRE_NEAR(body.bounds.topLeft.y, 4.0F);
        REQUIRE(contacts.left);
    }

    SECTION("right edge")
    {
        Body body;
        body.bounds = {{48.0F, 4.0F}, {8.0F, 8.0F}};
        body.velocity = {20.0F, 0.0F};
        const CollisionContacts contacts = simple_platformer::moveBody(map, body, 1.0F);
        REQUIRE_NEAR(body.bounds.topLeft.x, 56.0F);
        REQUIRE_NEAR(body.bounds.topLeft.y, 4.0F);
        REQUIRE(contacts.right);
    }

    SECTION("bottom edge")
    {
        Body body;
        body.bounds = {{4.0F, 32.0F}, {8.0F, 8.0F}};
        body.velocity = {0.0F, 20.0F};
        const CollisionContacts contacts = simple_platformer::moveBody(map, body, 1.0F);
        REQUIRE_NEAR(body.bounds.topLeft.x, 4.0F);
        REQUIRE_NEAR(body.bounds.topLeft.y, 40.0F);
        REQUIRE(contacts.ground);
    }
}

TEST_CASE("The top map edge stays open", "[physics][body][collision]")
{
    const TileMap map = tests::TileMapBuilder({"....", "....", "...."});
    Body body;
    body.bounds = {{8.0F, 0.0F}, {8.0F, 8.0F}};

    body.velocity = {0.0F, -40.0F};

    const CollisionContacts contacts = simple_platformer::moveBody(map, body, 1.0F);

    REQUIRE_NEAR(body.bounds.topLeft.x, 8.0F);
    REQUIRE_NEAR(body.bounds.topLeft.y, -40.0F);
    requireNoContacts(contacts);
}

TEST_CASE("Landing exactly on a floor reports ground contact", "[physics][body][collision]")
{
    const TileMap map = tests::TileMapBuilder({"....", "....", "####"});
    Body body;
    body.bounds = {{20.0F, 4.0F}, {12.0F, 12.0F}};

    body.velocity = {0.0F, 16.0F};

    const CollisionContacts contacts = simple_platformer::moveBody(map, body, 1.0F);

    REQUIRE_NEAR(body.bounds.topLeft.x, 20.0F);
    REQUIRE_NEAR(body.bounds.topLeft.y, 20.0F);
    REQUIRE(contacts.ground);
}

TEST_CASE("Arriving exactly at a map edge counts as touching it", "[physics][body][collision]")
{
    const TileMap map = tests::TileMapBuilder({"....", "....", "...."});

    SECTION("the left wall")
    {
        Body body;
        body.bounds = {{16.0F, 4.0F}, {8.0F, 8.0F}};
        body.velocity = {-16.0F, 0.0F};
        const CollisionContacts contacts = simple_platformer::moveBody(map, body, 1.0F);
        REQUIRE_NEAR(body.bounds.topLeft.x, 0.0F);
        REQUIRE(contacts.left);
    }

    SECTION("the right wall")
    {
        Body body;
        body.bounds = {{40.0F, 4.0F}, {8.0F, 8.0F}};
        body.velocity = {16.0F, 0.0F};
        const CollisionContacts contacts = simple_platformer::moveBody(map, body, 1.0F);
        REQUIRE_NEAR(body.bounds.topLeft.x, 56.0F);
        REQUIRE(contacts.right);
    }

    SECTION("the floor under the bottom row")
    {
        Body body;
        body.bounds = {{4.0F, 24.0F}, {8.0F, 8.0F}};
        body.velocity = {0.0F, 16.0F};
        const CollisionContacts contacts = simple_platformer::moveBody(map, body, 1.0F);
        REQUIRE_NEAR(body.bounds.topLeft.y, 40.0F);
        REQUIRE(contacts.ground);
    }

    SECTION("the open top, which is not a wall")
    {
        Body body;
        body.bounds = {{4.0F, 16.0F}, {8.0F, 8.0F}};
        body.velocity = {0.0F, -16.0F};
        const CollisionContacts contacts = simple_platformer::moveBody(map, body, 1.0F);
        REQUIRE_NEAR(body.bounds.topLeft.y, 0.0F);
        REQUIRE_FALSE(contacts.ceiling);
    }
}

TEST_CASE(
    "Sliding along a tile edge does not overlap the neighbouring row or column",
    "[physics][body][collision]")
{
    SECTION("horizontal movement along a floor")
    {
        const TileMap map = tests::TileMapBuilder({"....", "....", "####", "...."});
        for (const float speed : {-8.0F, 8.0F})
        {
            Body body{{{24.0F, 16.0F}, {8.0F, 16.0F}}, {speed, 0.0F}};
            const CollisionContacts contacts = simple_platformer::moveBody(map, body, 1.0F);
            REQUIRE_NEAR(body.bounds.topLeft.x, 24.0F + speed);
            REQUIRE_NEAR(body.velocity.x, speed);
            requireNoContacts(contacts);
        }
    }

    SECTION("vertical movement along a wall")
    {
        const TileMap map = tests::TileMapBuilder({"..#.", "..#.", "..#.", "..#."});
        for (const float speed : {-8.0F, 8.0F})
        {
            Body body{{{16.0F, 24.0F}, {16.0F, 8.0F}}, {0.0F, speed}};
            const CollisionContacts contacts = simple_platformer::moveBody(map, body, 1.0F);
            REQUIRE_NEAR(body.bounds.topLeft.y, 24.0F + speed);
            REQUIRE_NEAR(body.velocity.y, speed);
            requireNoContacts(contacts);
        }
    }
}

TEST_CASE("Very large finite movement respects map boundaries", "[physics][body][collision]")
{
    const TileMap map = tests::TileMapBuilder({"....", "....", "...."});
    const float largeMovement = std::numeric_limits<float>::max();

    SECTION("closed boundaries stop movement")
    {
        Body body;
        body.bounds = {{8.0F, 8.0F}, {8.0F, 8.0F}};

        body.velocity = {largeMovement, largeMovement};

        const CollisionContacts contacts = simple_platformer::moveBody(map, body, 1.0F);

        REQUIRE_NEAR(body.bounds.topLeft.x, 56.0F);
        REQUIRE_NEAR(body.bounds.topLeft.y, 40.0F);
        REQUIRE(contacts.right);
        REQUIRE(contacts.ground);
    }

    SECTION("the open top permits movement")
    {
        Body body;
        body.bounds = {{8.0F, 8.0F}, {8.0F, 8.0F}};

        body.velocity = {0.0F, -largeMovement};

        const CollisionContacts contacts = simple_platformer::moveBody(map, body, 1.0F);

        REQUIRE(body.bounds.topLeft.y == 8.0F - largeMovement);
        requireNoContacts(contacts);
    }
}

// Stationary contacts

TEST_CASE("Stationary bodies report the surfaces they touch", "[physics][body][collision]")
{
    const TileMap map = tests::TileMapBuilder({".##.", "#..#", "#..#", ".##."});
    const Aabb bounds{{16.0F, 16.0F}, {32.0F, 32.0F}};

    const CollisionContacts contacts = simple_platformer::touchingSurfaces(map, bounds);

    REQUIRE(contacts.left);
    REQUIRE(contacts.right);
    REQUIRE(contacts.ground);
    REQUIRE(contacts.ceiling);
    REQUIRE(bounds.topLeft.x == 16.0F);
    REQUIRE(bounds.topLeft.y == 16.0F);
}

TEST_CASE("Open space and the top map edge are not touching surfaces", "[physics][body][collision]")
{
    const TileMap map = tests::TileMapBuilder({"....", "....", "...."});
    const Aabb bounds{{16.0F, 0.0F}, {16.0F, 16.0F}};

    const CollisionContacts contacts = simple_platformer::touchingSurfaces(map, bounds);

    REQUIRE_FALSE(contacts.left);
    REQUIRE_FALSE(contacts.right);
    REQUIRE_FALSE(contacts.ground);
    REQUIRE_FALSE(contacts.ceiling);
}

TEST_CASE("Climbable contacts exclude ordinary solid tiles", "[physics][body][collision]")
{
    const TileMap map = tests::TileMapBuilder({".cc.", "X..c", "X..c", ".cc."})
                            .where('X', tests::Tile{}.blocksMovement())
                            .where('c', tests::Tile{}.blocksMovement().climbable());
    const Aabb bounds{{16.0F, 16.0F}, {32.0F, 32.0F}};

    const CollisionContacts physical = simple_platformer::touchingSurfaces(map, bounds);
    const CollisionContacts climbable = simple_platformer::touchingClimbableSurfaces(map, bounds);

    REQUIRE(physical.left);
    REQUIRE_FALSE(climbable.left);
    REQUIRE(climbable.right);
    REQUIRE(climbable.ceiling);
}

// Invalid collision inputs

TEST_CASE("Collision rejects invalid bounds and movement", "[physics][body][collision]")
{
    const TileMap map = tests::TileMapBuilder({"....", "....", "...."});

    Body emptyBody;
    emptyBody.bounds = {{0.0F, 0.0F}, {0.0F, 8.0F}};
    REQUIRE_THROWS_AS(simple_platformer::moveBody(map, emptyBody, 1.0F), std::invalid_argument);

    Body nonFiniteBody;
    nonFiniteBody.bounds = {{0.0F, 0.0F}, {8.0F, 8.0F}};
    nonFiniteBody.velocity = {std::numeric_limits<float>::infinity(), 0.0F};
    REQUIRE_THROWS_AS(simple_platformer::moveBody(map, nonFiniteBody, 1.0F), std::invalid_argument);
}
