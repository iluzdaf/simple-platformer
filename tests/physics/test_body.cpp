#include <catch2/catch_test_macros.hpp>

#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/physics/collision.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/require_near.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    using simple_platformer::Body;
    using simple_platformer::CollisionContacts;
    using simple_platformer::TileMap;
}

TEST_CASE("Gravity accelerates a body downwards until its fall speed", "[physics][body]")
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

TEST_CASE("A body moves by its velocity over the step", "[physics][body]")
{
    const TileMap map = tests::TileMapBuilder({"....", "....", "...."});
    Body body;
    body.bounds = {{8.0F, 8.0F}, {8.0F, 8.0F}};
    body.velocity = {120.0F, 90.0F};

    const CollisionContacts contacts = simple_platformer::moveBody(map, body, 0.1F);

    REQUIRE_NEAR(body.bounds.position.x, 20.0F);
    REQUIRE_NEAR(body.bounds.position.y, 17.0F);
    REQUIRE_NEAR(body.velocity.x, 120.0F);
    REQUIRE_NEAR(body.velocity.y, 90.0F);
    REQUIRE_FALSE(contacts.ground);
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
        REQUIRE_NEAR(body.bounds.position.y, 24.0F);
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
        REQUIRE_NEAR(body.bounds.position.x, 40.0F);
        REQUIRE_NEAR(body.velocity.x, 0.0F);
        REQUIRE_NEAR(body.velocity.y, -20.0F);
    }
}
