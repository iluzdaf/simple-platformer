#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <limits>
#include <stdexcept>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/physics/collision.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace
{
    using Catch::Matchers::WithinAbs;
    using simple_platformer::Aabb;
    using simple_platformer::CollisionContacts;
    using simple_platformer::TileMap;

    void requireVector(glm::vec2 actual, glm::vec2 expected)
    {
        REQUIRE_THAT(actual.x, WithinAbs(expected.x, 0.0001F));
        REQUIRE_THAT(actual.y, WithinAbs(expected.y, 0.0001F));
    }

    void requireNoContacts(const CollisionContacts& contacts)
    {
        REQUIRE_FALSE(contacts.left);
        REQUIRE_FALSE(contacts.right);
        REQUIRE_FALSE(contacts.ground);
        REQUIRE_FALSE(contacts.ceiling);
    }
}

TEST_CASE("An AABB moves freely through empty tiles", "[physics][collision]")
{
    const TileMap map = TileMap::fromAscii({"....", "....", "...."});
    Aabb bounds{{8.0F, 8.0F}, {8.0F, 8.0F}};

    const CollisionContacts contacts =
        simple_platformer::moveAndCollide(map, bounds, {12.0F, 9.0F});

    requireVector(bounds.position, {20.0F, 17.0F});
    requireNoContacts(contacts);
}

TEST_CASE("Horizontal movement stops on either side of a solid tile", "[physics][collision]")
{
    const TileMap map = TileMap::fromAscii({"..#...", "..#..."});

    SECTION("moving right")
    {
        Aabb bounds{{8.0F, 4.0F}, {8.0F, 20.0F}};
        const CollisionContacts contacts =
            simple_platformer::moveAndCollide(map, bounds, {30.0F, 0.0F});

        requireVector(bounds.position, {24.0F, 4.0F});
        REQUIRE(contacts.right);
    }

    SECTION("moving left")
    {
        Aabb bounds{{56.0F, 4.0F}, {8.0F, 20.0F}};
        const CollisionContacts contacts =
            simple_platformer::moveAndCollide(map, bounds, {-30.0F, 0.0F});

        requireVector(bounds.position, {48.0F, 4.0F});
        REQUIRE(contacts.left);
    }
}

TEST_CASE("Landing exactly on a floor reports ground contact", "[physics][collision]")
{
    const TileMap map = TileMap::fromAscii({"....", "....", "####"});
    Aabb bounds{{20.0F, 4.0F}, {12.0F, 12.0F}};

    const CollisionContacts contacts =
        simple_platformer::moveAndCollide(map, bounds, {0.0F, 16.0F});

    requireVector(bounds.position, {20.0F, 20.0F});
    REQUIRE(contacts.ground);
}

TEST_CASE("Vertical movement stops on floors and ceilings", "[physics][collision]")
{
    SECTION("falling onto a floor")
    {
        const TileMap map = TileMap::fromAscii({"....", "....", "####", "...."});
        Aabb bounds{{20.0F, 4.0F}, {12.0F, 12.0F}};
        const CollisionContacts contacts =
            simple_platformer::moveAndCollide(map, bounds, {0.0F, 30.0F});

        requireVector(bounds.position, {20.0F, 20.0F});
        REQUIRE(contacts.ground);
    }

    SECTION("jumping into a ceiling")
    {
        const TileMap map = TileMap::fromAscii({".##.", "....", "...."});
        Aabb bounds{{20.0F, 24.0F}, {12.0F, 8.0F}};
        const CollisionContacts contacts =
            simple_platformer::moveAndCollide(map, bounds, {0.0F, -20.0F});

        requireVector(bounds.position, {20.0F, 16.0F});
        REQUIRE(contacts.ceiling);
    }
}

TEST_CASE("Collision resolves X before Y at a corner", "[physics][collision]")
{
    const TileMap map = TileMap::fromAscii({"...", ".#.", "..."});
    Aabb bounds{{0.0F, 0.0F}, {8.0F, 8.0F}};

    const CollisionContacts contacts =
        simple_platformer::moveAndCollide(map, bounds, {16.0F, 16.0F});

    requireVector(bounds.position, {16.0F, 8.0F});
    REQUIRE(contacts.ground);
    REQUIRE_FALSE(contacts.right);
}

TEST_CASE("Collision supports bodies larger than one tile", "[physics][collision]")
{
    const TileMap map = TileMap::fromAscii({".....", ".....", ".....", "#####"});
    Aabb bounds{{18.0F, 2.0F}, {28.0F, 30.0F}};

    const CollisionContacts contacts =
        simple_platformer::moveAndCollide(map, bounds, {0.0F, 100.0F});

    requireVector(bounds.position, {18.0F, 18.0F});
    REQUIRE(contacts.ground);
}

TEST_CASE("Fast movement cannot pass through a solid tile", "[physics][collision]")
{
    const TileMap map = TileMap::fromAscii({"...#..", "...#.."});
    Aabb bounds{{4.0F, 4.0F}, {8.0F, 20.0F}};

    const CollisionContacts contacts =
        simple_platformer::moveAndCollide(map, bounds, {80.0F, 0.0F});

    requireVector(bounds.position, {40.0F, 4.0F});
    REQUIRE(contacts.right);
}

TEST_CASE("Very large finite movement respects map boundaries", "[physics][collision]")
{
    const TileMap map = TileMap::fromAscii({"....", "....", "...."});
    const float largeMovement = std::numeric_limits<float>::max();

    SECTION("closed boundaries stop movement")
    {
        Aabb bounds{{8.0F, 8.0F}, {8.0F, 8.0F}};

        const CollisionContacts contacts =
            simple_platformer::moveAndCollide(map, bounds, {largeMovement, largeMovement});

        requireVector(bounds.position, {56.0F, 40.0F});
        REQUIRE(contacts.right);
        REQUIRE(contacts.ground);
    }

    SECTION("the open top permits movement")
    {
        Aabb bounds{{8.0F, 8.0F}, {8.0F, 8.0F}};

        const CollisionContacts contacts =
            simple_platformer::moveAndCollide(map, bounds, {0.0F, -largeMovement});

        REQUIRE(bounds.position.y == 8.0F - largeMovement);
        requireNoContacts(contacts);
    }
}

TEST_CASE("The left right and bottom map edges are solid", "[physics][collision]")
{
    const TileMap map = TileMap::fromAscii({"....", "....", "...."});

    SECTION("left edge")
    {
        Aabb bounds{{4.0F, 4.0F}, {8.0F, 8.0F}};
        const CollisionContacts contacts =
            simple_platformer::moveAndCollide(map, bounds, {-20.0F, 0.0F});
        requireVector(bounds.position, {0.0F, 4.0F});
        REQUIRE(contacts.left);
    }

    SECTION("right edge")
    {
        Aabb bounds{{48.0F, 4.0F}, {8.0F, 8.0F}};
        const CollisionContacts contacts =
            simple_platformer::moveAndCollide(map, bounds, {20.0F, 0.0F});
        requireVector(bounds.position, {56.0F, 4.0F});
        REQUIRE(contacts.right);
    }

    SECTION("bottom edge")
    {
        Aabb bounds{{4.0F, 32.0F}, {8.0F, 8.0F}};
        const CollisionContacts contacts =
            simple_platformer::moveAndCollide(map, bounds, {0.0F, 20.0F});
        requireVector(bounds.position, {4.0F, 40.0F});
        REQUIRE(contacts.ground);
    }
}

TEST_CASE("The top map edge stays open", "[physics][collision]")
{
    const TileMap map = TileMap::fromAscii({"....", "....", "...."});
    Aabb bounds{{8.0F, 0.0F}, {8.0F, 8.0F}};

    const CollisionContacts contacts =
        simple_platformer::moveAndCollide(map, bounds, {0.0F, -40.0F});

    requireVector(bounds.position, {8.0F, -40.0F});
    requireNoContacts(contacts);
}

TEST_CASE("Collision rejects invalid bounds and movement", "[physics][collision]")
{
    const TileMap map = TileMap::fromAscii({"....", "....", "...."});

    Aabb emptyBounds{{0.0F, 0.0F}, {0.0F, 8.0F}};
    REQUIRE_THROWS_AS(
        simple_platformer::moveAndCollide(map, emptyBounds, {0.0F, 0.0F}), std::invalid_argument);

    Aabb finiteBounds{{0.0F, 0.0F}, {8.0F, 8.0F}};
    REQUIRE_THROWS_AS(
        simple_platformer::moveAndCollide(
            map, finiteBounds, {std::numeric_limits<float>::infinity(), 0.0F}),
        std::invalid_argument);
}
