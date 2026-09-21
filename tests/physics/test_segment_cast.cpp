#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <stdexcept>
#include <optional>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/physics/segment_cast.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/tile_map_builder.hpp"

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
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".x.x.", "....."})
                                               .where('x', tests::Tile().blocksMovement());

    const std::optional<simple_platformer::TileSegmentHit> hit =
        simple_platformer::segmentCastMovementBlockingTiles(map, {0.0F, 24.0F}, {80.0F, 24.0F});

    if (!hit)
    {
        throw std::logic_error("Expected the cast to hit a tile");
    }
    REQUIRE_THAT(hit->segmentTime, Catch::Matchers::WithinAbs(0.2F, 0.0001F));
    REQUIRE(hit->cell == simple_platformer::GridPosition{1, 1});
}

TEST_CASE("A solid tile cast accounts for the moving box size", "[physics][segment][tile]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", "..x..", "....."})
                                               .where('x', tests::Tile().blocksMovement());
    const glm::vec2 start = {0.0F, 8.0F};
    const glm::vec2 end = {64.0F, 8.0F};

    REQUIRE_FALSE(simple_platformer::segmentCastMovementBlockingTiles(map, start, end));
    REQUIRE(simple_platformer::segmentCastMovementBlockingTiles(map, start, end, {4.0F, 16.0F})
                .has_value());
}

TEST_CASE(
    "A sight cast ignores the cover it starts in until it reaches open ground",
    "[physics][segment][tile]")
{
    const simple_platformer::TileMap onePatch =
        tests::TileMapBuilder({".....", "ccc..", "....."}).where('c', tests::Tile().blocksSight());
    const simple_platformer::TileMap twoPatches =
        tests::TileMapBuilder({".....", "cc.c.", "....."}).where('c', tests::Tile().blocksSight());

    REQUIRE_FALSE(
        simple_platformer::segmentCastSightBlockingTiles(onePatch, {8.0F, 24.0F}, {72.0F, 24.0F}));
    const std::optional<float> hit =
        simple_platformer::segmentCastSightBlockingTiles(twoPatches, {8.0F, 24.0F}, {72.0F, 24.0F});
    REQUIRE(hit.has_value());
    REQUIRE_THAT(hit.value_or(-1.0F), Catch::Matchers::WithinAbs(0.625F, 0.0001F));
}

TEST_CASE(
    "A sight cast starting on the edge of cover counts as starting in it",
    "[physics][segment][tile]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({".....", ".c...", "....."}).where('c', tests::Tile().blocksSight());

    REQUIRE_FALSE(
        simple_platformer::segmentCastSightBlockingTiles(map, {16.0F, 24.0F}, {72.0F, 24.0F}));
    REQUIRE_FALSE(
        simple_platformer::segmentCastSightBlockingTiles(map, {32.0F, 24.0F}, {72.0F, 24.0F}));
}

TEST_CASE(
    "A sight cast joins cover tiles only through the exact corner they share",
    "[physics][segment][tile]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({".c...", "c....", "....."}).where('c', tests::Tile().blocksSight());

    // Through the shared corner at (16, 16), the line never reaches open ground.
    REQUIRE_FALSE(
        simple_platformer::segmentCastSightBlockingTiles(map, {8.0F, 24.0F}, {24.0F, 8.0F}));

    // Just beside it, the line crosses the open tile below the far cover first.
    const std::optional<float> hit =
        simple_platformer::segmentCastSightBlockingTiles(map, {8.0F, 24.0F}, {28.0F, 8.0F});
    REQUIRE(hit.has_value());
    REQUIRE_THAT(hit.value_or(-1.0F), Catch::Matchers::WithinAbs(0.5F, 0.0001F));
}

TEST_CASE("Solid tile casts reject an invalid moving size", "[physics][segment][tile]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"..."});

    REQUIRE_THROWS_AS(
        simple_platformer::segmentCastMovementBlockingTiles(
            map, {0.0F, 0.0F}, {16.0F, 0.0F}, {-1.0F, 0.0F}),
        std::invalid_argument);
}
