#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/platformer_cells.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"

namespace
{
    using simple_platformer::GridPosition;

    constexpr glm::vec2 SmallBody{12.0F, 12.0F};
    constexpr glm::vec2 TallBody{12.0F, 20.0F};
}

TEST_CASE("A standable cell has support below and room for the body", "[navigation][platformer]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".#.", "...", "###"});

    REQUIRE(simple_platformer::canStandAt(map, {1, 1}, SmallBody));
    // The tall body would reach into the tile above.
    REQUIRE_FALSE(simple_platformer::canStandAt(map, {1, 1}, TallBody));
    REQUIRE_FALSE(simple_platformer::canStandAt(map, {1, 0}, SmallBody));
}

TEST_CASE("Platformer neighbors include every other standable cell", "[navigation][platformer]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"..#..", "#####"});

    REQUIRE(
        simple_platformer::platformerNeighbors(map, {0, 0}, SmallBody) ==
        std::vector<GridPosition>{{1, 0}, {3, 0}, {4, 0}});
    REQUIRE(simple_platformer::platformerNeighbors(map, {2, 0}, SmallBody).empty());
}

TEST_CASE(
    "A platformer start cell is the standable cell that supports the body",
    "[navigation][platformer]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "..###..."});

    REQUIRE(
        simple_platformer::findPlatformerStartCell(
            map, simple_platformer::boxInCell(tests::TileSize, {3, 1}, TallBody)) ==
        GridPosition{3, 1});
    // A body wider than a tile is still placed by its feet.
    REQUIRE(
        simple_platformer::findPlatformerStartCell(
            map, simple_platformer::boxInCell(tests::TileSize, {3, 1}, {20.0F, 20.0F})) ==
        GridPosition{3, 1});

    // At the ledge the feet hang past the platform, so the cell under them cannot be
    // stood on; the start is the supporting cell the collider still rests on.
    simple_platformer::Aabb hanging{{0.0F, 0.0F}, TallBody};
    simple_platformer::placeFeetAt(hanging, {80.5F, 32.0F});
    REQUIRE(
        simple_platformer::cellAtFeet(tests::TileSize, simple_platformer::feetOf(hanging)) ==
        GridPosition{5, 1});
    REQUIRE(simple_platformer::findPlatformerStartCell(map, hanging) == GridPosition{4, 1});

    REQUIRE(
        simple_platformer::findPlatformerStartCell(
            map, simple_platformer::boxInCell(tests::TileSize, {0, 0}, TallBody)) == std::nullopt);

    REQUIRE_THROWS_AS(
        simple_platformer::findPlatformerStartCell(map, {{0.0F, 0.0F}, {0.0F, 20.0F}}),
        std::invalid_argument);
}

TEST_CASE(
    "The feet cell is used when standable, otherwise the nearest standable cell",
    "[navigation][platformer]")
{
    const simple_platformer::TileMap platform =
        tests::TileMapBuilder({"........", "........", "..###...", "........", "########"});
    REQUIRE(
        simple_platformer::findNearestStandableCell(platform, {47.5F, 32.0F}, TallBody) ==
        GridPosition{2, 1});
    // Past either edge, or in the air above it, the nearest cell of the platform.
    REQUIRE(
        simple_platformer::findNearestStandableCell(platform, {31.5F, 32.0F}, TallBody) ==
        GridPosition{2, 1});
    REQUIRE(
        simple_platformer::findNearestStandableCell(platform, {80.5F, 32.0F}, TallBody) ==
        GridPosition{4, 1});
    REQUIRE(
        simple_platformer::findNearestStandableCell(platform, {31.0F, 20.0F}, TallBody) ==
        GridPosition{2, 1});

    // The requested body size decides what is standable: the tall body cannot fit under
    // the ceiling, and equal distances keep row, then column order, so the cell on the
    // left wins the tie.
    const simple_platformer::TileMap ceiling =
        tests::TileMapBuilder({"..#....", ".......", "#######"});
    REQUIRE(
        simple_platformer::findNearestStandableCell(ceiling, {40.0F, 32.0F}, SmallBody) ==
        GridPosition{2, 1});
    REQUIRE(
        simple_platformer::findNearestStandableCell(ceiling, {40.0F, 32.0F}, TallBody) ==
        GridPosition{1, 1});
    REQUIRE(
        simple_platformer::findNearestStandableCell(ceiling, {8.0F, 32.0F}, {20.0F, 12.0F}) ==
        GridPosition{1, 1});

    // Feet off the map still find the nearest cell; a map with nowhere to stand has none.
    REQUIRE(
        simple_platformer::findNearestStandableCell(ceiling, {-16.0F, 32.0F}, TallBody) ==
        GridPosition{0, 1});
    const simple_platformer::TileMap solid = tests::TileMapBuilder({"###", "###"});
    REQUIRE_FALSE(simple_platformer::findNearestStandableCell(solid, {24.0F, 16.0F}, TallBody));
}

TEST_CASE("Equidistant standable cells prefer the upper row", "[navigation][platformer]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({".....", ".....", "..#..", ".....", "..#.."});

    REQUIRE(
        simple_platformer::findNearestStandableCell(map, {40.0F, 48.0F}, SmallBody) ==
        GridPosition{2, 1});
}

TEST_CASE("The nearest standable cell can be in a later row", "[navigation][platformer]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({".....", ".....", "##...", ".....", "#####"});

    REQUIRE(
        simple_platformer::findNearestStandableCell(map, {40.0F, 48.0F}, SmallBody) ==
        GridPosition{2, 3});
}

TEST_CASE(
    "Finding the nearest standable cell rejects invalid feet and body sizes",
    "[navigation][platformer][validation]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    REQUIRE_THROWS_AS(
        simple_platformer::findNearestStandableCell(map, {infinity, 32.0F}, TallBody),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findNearestStandableCell(map, {24.0F, nan}, TallBody),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findNearestStandableCell(map, {24.0F, 32.0F}, {0.0F, 20.0F}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findNearestStandableCell(map, {24.0F, 32.0F}, {12.0F, -1.0F}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findNearestStandableCell(map, {24.0F, 32.0F}, {infinity, 20.0F}),
        std::invalid_argument);
}
