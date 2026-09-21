#include <catch2/catch_test_macros.hpp>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/world/player_sight.hpp"
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

    simple_platformer::Actor playerInCell(int column)
    {
        return tests::ActorBuilder::walking(boxInCell(column));
    }
}

TEST_CASE("Grass hides what stands in it from a player outside it", "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "...cc...", "........"})
            .where('c', tests::Tile().blocksSight());
    const simple_platformer::Actor player = playerInCell(0);

    REQUIRE_FALSE(simple_platformer::playerCanSee(map, &player, boxInCell(3)));
    REQUIRE_FALSE(simple_platformer::playerCanSee(map, &player, boxInCell(4)));
}

TEST_CASE("Grass hides nothing standing outside it", "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "...cc...", "........"})
            .where('c', tests::Tile().blocksSight());
    const simple_platformer::Actor player = playerInCell(0);

    // The grass between them does not matter: only standing in grass hides.
    REQUIRE(simple_platformer::playerCanSee(map, &player, boxInCell(7)));
}

TEST_CASE("A player in grass sees its own patch but not another", "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "...ccc.c", "........"})
            .where('c', tests::Tile().blocksSight());
    const simple_platformer::Actor player = playerInCell(3);

    REQUIRE(simple_platformer::playerCanSee(map, &player, boxInCell(5)));
    REQUIRE_FALSE(simple_platformer::playerCanSee(map, &player, boxInCell(7)));
}

TEST_CASE("Without a player, nobody sees into grass", "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"....", ".cc.", "...."}).where('c', tests::Tile().blocksSight());

    REQUIRE_FALSE(simple_platformer::playerCanSee(map, nullptr, boxInCell(1)));
    REQUIRE(simple_platformer::playerCanSee(map, nullptr, boxInCell(3)));
}
