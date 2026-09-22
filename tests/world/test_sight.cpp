#include <catch2/catch_test_macros.hpp>

#include <optional>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/world/sight.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"

namespace
{
    simple_platformer::Aabb boxIn(simple_platformer::GridPosition cell)
    {
        return simple_platformer::boxInCell(tests::TileSize, cell, {12.0F, 12.0F});
    }
}

TEST_CASE("Cover hides what stands in it from a viewer outside it", "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "...cc...", "........"})
            .where('c', tests::Tile().blocksSight());
    const glm::vec2 viewer = simple_platformer::centerOf(boxIn({0, 1}));

    REQUIRE(simple_platformer::hiddenByCover(map, viewer, boxIn({3, 1})));
    REQUIRE(simple_platformer::hiddenByCover(map, viewer, boxIn({4, 1})));
}

TEST_CASE("Cover hides nothing standing outside it", "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "...cc...", "........"})
            .where('c', tests::Tile().blocksSight());
    const glm::vec2 viewer = simple_platformer::centerOf(boxIn({0, 1}));

    // The cover between them does not matter: only standing in cover hides.
    REQUIRE_FALSE(simple_platformer::hiddenByCover(map, viewer, boxIn({7, 1})));
}

TEST_CASE("A viewer in cover sees its own patch but not another", "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "...ccc.c", "........"})
            .where('c', tests::Tile().blocksSight());
    const glm::vec2 viewer = simple_platformer::centerOf(boxIn({3, 1}));

    REQUIRE_FALSE(simple_platformer::hiddenByCover(map, viewer, boxIn({5, 1})));
    REQUIRE(simple_platformer::hiddenByCover(map, viewer, boxIn({7, 1})));
}

TEST_CASE("Without a viewer, everything in cover is hidden", "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"....", ".cc.", "...."}).where('c', tests::Tile().blocksSight());

    REQUIRE(simple_platformer::hiddenByCover(map, std::nullopt, boxIn({1, 1})));
    REQUIRE_FALSE(simple_platformer::hiddenByCover(map, std::nullopt, boxIn({3, 1})));
}

TEST_CASE("A wall breaks line of sight but hides nothing in the open", "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({".....", "..w..", "....."})
            .where('w', tests::Tile().blocksMovement().blocksSight());
    const glm::vec2 viewer = simple_platformer::centerOf(boxIn({0, 1}));
    const simple_platformer::Aabb target = boxIn({4, 1});

    REQUIRE_FALSE(simple_platformer::lineOfSight(map, viewer, simple_platformer::centerOf(target)));
    REQUIRE_FALSE(simple_platformer::hiddenByCover(map, viewer, target));
}
