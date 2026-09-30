#include <catch2/catch_test_macros.hpp>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/world/sight.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"

namespace
{
    // Where an actor standing in the cell sees from: the centre of its body.
    glm::vec2 centerOfBodyIn(simple_platformer::Cell cell)
    {
        return simple_platformer::centerOf(
            simple_platformer::boxInCell(tests::TileSize, cell, {12.0F, 12.0F}));
    }
}

TEST_CASE("A wall between two points breaks line of sight", "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({".....", "..w..", "....."})
            .where('w', tests::Tile().blocksMovement().blocksSight());

    REQUIRE_FALSE(
        simple_platformer::lineOfSight(map, centerOfBodyIn({0, 1}), centerOfBodyIn({4, 1})));
    REQUIRE(simple_platformer::lineOfSight(map, centerOfBodyIn({0, 2}), centerOfBodyIn({4, 2})));
}

TEST_CASE("A viewer in cover sees out of it but not into other cover", "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({".........", "ccc.....c", "........."})
            .where('c', tests::Tile().blocksSight());

    // The cover the line starts in does not block it, so the viewer sees across its patch
    // and out into the open.
    REQUIRE(simple_platformer::lineOfSight(map, centerOfBodyIn({0, 1}), centerOfBodyIn({2, 1})));
    REQUIRE(simple_platformer::lineOfSight(map, centerOfBodyIn({0, 1}), centerOfBodyIn({5, 1})));
    // Entering another patch after a gap does.
    REQUIRE_FALSE(
        simple_platformer::lineOfSight(map, centerOfBodyIn({0, 1}), centerOfBodyIn({8, 1})));
}
