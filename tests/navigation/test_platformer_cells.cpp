#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
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

    // Reference oracle: prefer a standable target cell, then scan in row-column order
    // for the nearest standable cell so ties keep the first cell found.
    std::optional<GridPosition> chaseCellByExhaustiveScan(
        const simple_platformer::TileMap& map,
        glm::vec2 feet,
        glm::vec2 bodySize)
    {
        if (feet.x >= 0.0F && feet.x < map.pixelWidth() && feet.y >= 0.0F &&
            feet.y <= map.pixelHeight())
        {
            const GridPosition targetCell = simple_platformer::cellAtFeet(map.tileSize(), feet);
            if (simple_platformer::canStandAt(map, targetCell, bodySize))
            {
                return targetCell;
            }
        }
        std::optional<GridPosition> closest;
        double closestDistanceSquared = 0.0;
        for (int row = 0; row < map.height(); ++row)
        {
            for (int column = 0; column < map.width(); ++column)
            {
                const GridPosition candidate{column, row};
                if (!simple_platformer::canStandAt(map, candidate, bodySize))
                {
                    continue;
                }
                const glm::vec2 candidateFeet =
                    simple_platformer::feetInCell(map.tileSize(), candidate);
                const double dx = static_cast<double>(candidateFeet.x) - feet.x;
                const double dy = static_cast<double>(candidateFeet.y) - feet.y;
                const double distanceSquared = dx * dx + dy * dy;
                if (!closest.has_value() || distanceSquared < closestDistanceSquared)
                {
                    closest = candidate;
                    closestDistanceSquared = distanceSquared;
                }
            }
        }
        return closest;
    }
}

TEST_CASE("A standable cell has support below and room for the body", "[navigation][platformer]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".#.", "...", "###"});

    REQUIRE(simple_platformer::canStandAt(map, {1, 1}, SmallBody));
    // The tall body would reach into the tile above.
    REQUIRE_FALSE(simple_platformer::canStandAt(map, {1, 1}, TallBody));
    REQUIRE_FALSE(simple_platformer::canStandAt(map, {1, 0}, SmallBody));
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
    "A chase cell is the target's cell when standable, else the nearest standable cell",
    "[navigation][platformer]")
{
    const simple_platformer::TileMap platform =
        tests::TileMapBuilder({"........", "........", "..###...", "........", "########"});
    // Standing on the platform, its own cell.
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(platform, {47.5F, 32.0F}, TallBody) ==
        GridPosition{2, 1});
    // Past either edge, or in the air above it, the nearest cell of the platform.
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(platform, {31.5F, 32.0F}, TallBody) ==
        GridPosition{2, 1});
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(platform, {80.5F, 32.0F}, TallBody) ==
        GridPosition{4, 1});
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(platform, {31.0F, 20.0F}, TallBody) ==
        GridPosition{2, 1});

    // The pursuer's own body decides what is standable: the tall body cannot fit under
    // the ceiling, and equal distances keep row, then column order, so the cell on the
    // left wins the tie.
    const simple_platformer::TileMap ceiling =
        tests::TileMapBuilder({"..#....", ".......", "#######"});
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(ceiling, {40.0F, 32.0F}, SmallBody) ==
        GridPosition{2, 1});
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(ceiling, {40.0F, 32.0F}, TallBody) ==
        GridPosition{1, 1});
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(ceiling, {8.0F, 32.0F}, {20.0F, 12.0F}) ==
        GridPosition{1, 1});

    // Feet off the map still find the nearest cell; a map with nowhere to stand has none.
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(ceiling, {-16.0F, 32.0F}, TallBody) ==
        GridPosition{0, 1});
    const simple_platformer::TileMap solid = tests::TileMapBuilder({"###", "###"});
    REQUIRE_FALSE(simple_platformer::findPlatformerChaseCell(solid, {24.0F, 16.0F}, TallBody));
}

TEST_CASE("Chase cells match an exhaustive map-scan oracle", "[navigation][platformer]")
{
    const std::vector<std::vector<std::string>> maps = {
        {"........", "........", "..###...", "........", "########"},
        {"..#....", ".......", "#######"},
        {"##########", "#........#", "#..##..#.#", "#......#.#", "##########"},
        {"...", "...", "..."},
    };
    const std::vector<glm::vec2> bodies = {SmallBody, TallBody, {20.0F, 12.0F}};
    for (const std::vector<std::string>& mapRows : maps)
    {
        const simple_platformer::TileMap map = tests::TileMapBuilder(mapRows);
        // Sample feet on a fine grid over the map and a margin outside it.
        const int height = static_cast<int>(map.pixelHeight());
        const int width = static_cast<int>(map.pixelWidth());
        for (int y = -24; y <= height + 24; y += 5)
        {
            for (int x = -24; x <= width + 24; x += 5)
            {
                const glm::vec2 feet{static_cast<float>(x), static_cast<float>(y)};
                for (const glm::vec2 body : bodies)
                {
                    CAPTURE(mapRows, feet.x, feet.y, body.x, body.y);
                    REQUIRE(
                        simple_platformer::findPlatformerChaseCell(map, feet, body) ==
                        chaseCellByExhaustiveScan(map, feet, body));
                }
            }
        }
    }
}

TEST_CASE("Chase cells reject invalid feet and bodies", "[navigation][platformer][validation]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    REQUIRE_THROWS_AS(
        simple_platformer::findPlatformerChaseCell(map, {infinity, 32.0F}, TallBody),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findPlatformerChaseCell(map, {24.0F, nan}, TallBody),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findPlatformerChaseCell(map, {24.0F, 32.0F}, {0.0F, 20.0F}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findPlatformerChaseCell(map, {24.0F, 32.0F}, {12.0F, -1.0F}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findPlatformerChaseCell(map, {24.0F, 32.0F}, {infinity, 20.0F}),
        std::invalid_argument);
}
