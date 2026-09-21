#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include <glm/vec2.hpp>

#include "simple_platformer/world/tile_map.hpp"
#include "support/tile_map_builder.hpp"

TEST_CASE("'.' is empty and '#' blocks movement and sight", "[support][tile-map-builder]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".#"});

    REQUIRE(map.tileAt({0, 0}) == 0);
    REQUIRE_FALSE(map.blocksMovement({0, 0}));
    REQUIRE_FALSE(map.blocksSight({0, 0}));
    REQUIRE(map.blocksMovement({1, 0}));
    REQUIRE(map.blocksSight({1, 0}));
}

TEST_CASE("Declared tiles block only what they declare", "[support][tile-map-builder]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"gw"})
                                               .where('g', tests::Tile().blocksSight())
                                               .where('w', tests::Tile().blocksMovement());

    REQUIRE_FALSE(map.blocksMovement({0, 0}));
    REQUIRE(map.blocksSight({0, 0}));
    REQUIRE(map.blocksMovement({1, 0}));
    REQUIRE_FALSE(map.blocksSight({1, 0}));
}

TEST_CASE(
    "Declared tiles take IDs from 1 in order and '#' the next one",
    "[support][tile-map-builder]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"ba#"}).where('b', tests::Tile()).where('a', tests::Tile());

    REQUIRE(map.tileAt({0, 0}) == 1);
    REQUIRE(map.tileAt({1, 0}) == 2);
    REQUIRE(map.tileAt({2, 0}) == 3);
}

TEST_CASE("A declared tile keeps its sprite region", "[support][tile-map-builder]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"s"}).where(
        's', tests::Tile().withSprite({{16.0F, 32.0F}, {16.0F, 16.0F}}));

    REQUIRE(map.definitionAt({0, 0}).sprite.position == glm::vec2{16.0F, 32.0F});
    REQUIRE(map.definitionAt({0, 0}).sprite.size == glm::vec2{16.0F, 16.0F});
}

TEST_CASE("A breakable tile breaks into the tile its symbol names", "[support][tile-map-builder]")
{
    simple_platformer::TileMap map = tests::TileMapBuilder({"cr"})
                                         .where('c', tests::Tile().breaksInto('r'))
                                         .where('r', tests::Tile().breaksInto('.'));

    REQUIRE(map.breakTile({0, 0}));
    REQUIRE(map.tileAt({0, 0}) == map.tileAt({1, 0}));
    REQUIRE(map.breakTile({1, 0}));
    REQUIRE(map.tileAt({1, 0}) == 0);
}

TEST_CASE("The tile map builder rejects ambiguous symbols", "[support][tile-map-builder]")
{
    REQUIRE_THROWS_AS(
        tests::TileMapBuilder({"."}).where('.', tests::Tile().blocksSight()), std::logic_error);
    REQUIRE_THROWS_AS(tests::TileMapBuilder({"#"}).where('#', tests::Tile()), std::logic_error);
    REQUIRE_THROWS_AS(
        tests::TileMapBuilder({"g"})
            .where('g', tests::Tile().blocksSight())
            .where('g', tests::Tile().blocksMovement()),
        std::logic_error);
}

TEST_CASE("The tile map builder rejects symbols it has not declared", "[support][tile-map-builder]")
{
    const auto usedButUndeclared = []
    {
        const simple_platformer::TileMap map = tests::TileMapBuilder({".g"});
        return map.width();
    };
    const auto breaksIntoUndeclared = []
    {
        const simple_platformer::TileMap map =
            tests::TileMapBuilder({"c"}).where('c', tests::Tile().breaksInto('r'));
        return map.width();
    };

    REQUIRE_THROWS_AS(usedButUndeclared(), std::invalid_argument);
    REQUIRE_THROWS_AS(breaksIntoUndeclared(), std::logic_error);
}
