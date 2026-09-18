#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <stdexcept>
#include <optional>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/physics/segment_cast.hpp"
#include "simple_platformer/world/tile_map.hpp"

TEST_CASE("A segment cast reports its first entry into an AABB", "[physics][segment]")
{
    const simple_platformer::Aabb box{{10.0F, 10.0F}, {10.0F, 10.0F}};
    const std::optional<float> hit =
        simple_platformer::segmentCast(box, {0.0F, 15.0F}, {40.0F, 15.0F});

    REQUIRE(hit.has_value());
    REQUIRE_THAT(hit.value_or(-1.0F), Catch::Matchers::WithinAbs(0.25F, 0.0001F));
}

TEST_CASE("A segment can miss or begin inside an AABB", "[physics][segment]")
{
    const simple_platformer::Aabb box{{10.0F, 10.0F}, {10.0F, 10.0F}};

    REQUIRE_FALSE(simple_platformer::segmentCast(box, {0.0F, 5.0F}, {40.0F, 5.0F}));
    REQUIRE(simple_platformer::segmentCast(box, {15.0F, 15.0F}, {40.0F, 15.0F}) == 0.0F);
}

TEST_CASE("Segment casts reject invalid data", "[physics][segment]")
{
    const simple_platformer::Aabb empty{{0.0F, 0.0F}, {0.0F, 10.0F}};
    REQUIRE_THROWS_AS(
        simple_platformer::segmentCast(empty, {0.0F, 0.0F}, {1.0F, 1.0F}), std::invalid_argument);
}

TEST_CASE("A solid tile cast reports the earliest tile", "[physics][segment][tile]")
{
    const simple_platformer::TileMap map =
        simple_platformer::TileMap::fromAscii({".....", ".#.#.", "....."});

    const std::optional<float> hit =
        simple_platformer::segmentCastMovementBlockingTiles(map, {0.0F, 24.0F}, {80.0F, 24.0F});

    REQUIRE(hit.has_value());
    REQUIRE_THAT(hit.value_or(-1.0F), Catch::Matchers::WithinAbs(0.2F, 0.0001F));
}

TEST_CASE("A solid tile cast accounts for the moving box size", "[physics][segment][tile]")
{
    const simple_platformer::TileMap map =
        simple_platformer::TileMap::fromAscii({".....", "..#..", "....."});
    const glm::vec2 start = {0.0F, 8.0F};
    const glm::vec2 end = {64.0F, 8.0F};

    REQUIRE_FALSE(simple_platformer::segmentCastMovementBlockingTiles(map, start, end));
    REQUIRE(simple_platformer::segmentCastMovementBlockingTiles(map, start, end, {4.0F, 16.0F})
                .has_value());
}

TEST_CASE("Solid tile casts reject an invalid moving size", "[physics][segment][tile]")
{
    const simple_platformer::TileMap map = simple_platformer::TileMap::fromAscii({"..."});

    REQUIRE_THROWS_AS(
        simple_platformer::segmentCastMovementBlockingTiles(
            map, {0.0F, 0.0F}, {16.0F, 0.0F}, {-1.0F, 0.0F}),
        std::invalid_argument);
}
