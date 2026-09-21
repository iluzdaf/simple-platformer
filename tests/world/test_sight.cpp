#include <catch2/catch_test_macros.hpp>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/world/sight.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/actor_builder.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    // A 12-pixel box centred on a cell of row 1.
    simple_platformer::Aabb boxInCell(int column)
    {
        return {{static_cast<float>(column * 16 + 2), 18.0F}, {12.0F, 12.0F}};
    }

    simple_platformer::Actor viewerInCell(int column)
    {
        return tests::ActorBuilder::walking(boxInCell(column));
    }
}

TEST_CASE("Cover hides what stands in it from a viewer outside it", "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "...cc...", "........"})
            .where('c', tests::Tile().blocksSight());
    const simple_platformer::Actor viewer = viewerInCell(0);

    REQUIRE(simple_platformer::hiddenByCover(map, &viewer, boxInCell(3)));
    REQUIRE(simple_platformer::hiddenByCover(map, &viewer, boxInCell(4)));
}

TEST_CASE("Cover hides nothing standing outside it", "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "...cc...", "........"})
            .where('c', tests::Tile().blocksSight());
    const simple_platformer::Actor viewer = viewerInCell(0);

    // The cover between them does not matter: only standing in cover hides.
    REQUIRE_FALSE(simple_platformer::hiddenByCover(map, &viewer, boxInCell(7)));
}

TEST_CASE("A viewer in cover sees its own patch but not another", "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "...ccc.c", "........"})
            .where('c', tests::Tile().blocksSight());
    const simple_platformer::Actor viewer = viewerInCell(3);

    REQUIRE_FALSE(simple_platformer::hiddenByCover(map, &viewer, boxInCell(5)));
    REQUIRE(simple_platformer::hiddenByCover(map, &viewer, boxInCell(7)));
}

TEST_CASE("Without a viewer, everything in cover is hidden", "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"....", ".cc.", "...."}).where('c', tests::Tile().blocksSight());

    REQUIRE(simple_platformer::hiddenByCover(map, nullptr, boxInCell(1)));
    REQUIRE_FALSE(simple_platformer::hiddenByCover(map, nullptr, boxInCell(3)));
}

TEST_CASE("A wall breaks line of sight but hides nothing in the open", "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({".....", "..w..", "....."})
            .where('w', tests::Tile().blocksMovement().blocksSight());
    const simple_platformer::Actor viewer = viewerInCell(0);
    const simple_platformer::Aabb target = boxInCell(4);

    REQUIRE_FALSE(simple_platformer::lineOfSight(
        map, simple_platformer::centerOf(viewer.body.bounds), simple_platformer::centerOf(target)));
    REQUIRE_FALSE(simple_platformer::hiddenByCover(map, &viewer, target));
}
