#include <catch2/catch_test_macros.hpp>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/navigation/route.hpp"
#include "simple_platformer/navigation/platformer_cells.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"

namespace
{
    using simple_platformer::Cell;

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

TEST_CASE("A body may extend above the map but not through its walls", "[navigation][platformer]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", "####"});
    const glm::vec2 tall{12.0F, 40.0F};
    REQUIRE(simple_platformer::boxInCell(tests::TileSize, {1, 0}, tall).topLeft.y < 0.0F);
    REQUIRE(simple_platformer::canStandAt(map, {1, 0}, tall));

    // Wider than a tile in the first column, the body would poke past the map's side.
    REQUIRE_FALSE(simple_platformer::canStandAt(map, {0, 0}, {20.0F, 12.0F}));
}

TEST_CASE("A wall location may extend above the open map top", "[navigation][platformer][climb]")
{
    using simple_platformer::ClimbSurface;
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({".c..", ".c..", "####"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    const glm::vec2 tall{12.0F, 40.0F};
    const simple_platformer::RouteLocation onWall{{2, 0}, ClimbSurface::LeftWall};

    REQUIRE(simple_platformer::boundsAtSurface(tests::TileSize, onWall, tall).topLeft.y < 0.0F);
    REQUIRE(simple_platformer::canOccupy(map, onWall, tall));
}

TEST_CASE(
    "A climbable cell has a climbable wall or ceiling the body can rest on",
    "[navigation][platformer][climb]")
{
    // A climbable roof and left wall, and a plain wall on the right.
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"ccccc", "c...#", "c...#", "#####"})
            .where('c', tests::Tile{}.blocksMovement().climbable());

    // Beside the climbable wall, and under the roof.
    REQUIRE(simple_platformer::canClimbAt(map, {1, 2}, SmallBody));
    REQUIRE(simple_platformer::canClimbAt(map, {2, 1}, SmallBody));
    // In the open, and beside a wall that cannot be climbed.
    REQUIRE_FALSE(simple_platformer::canClimbAt(map, {2, 2}, SmallBody));
    REQUIRE_FALSE(simple_platformer::canClimbAt(map, {3, 2}, SmallBody));
    // Inside the wall itself.
    REQUIRE_FALSE(simple_platformer::canClimbAt(map, {0, 2}, SmallBody));
}
