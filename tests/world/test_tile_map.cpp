#include <catch2/catch_test_macros.hpp>

#include <map>
#include <stdexcept>
#include <vector>

#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"

TEST_CASE("An ASCII tile map is rectangular and row-major", "[world][tile-map]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".#.", "##."});

    REQUIRE(map.width() == 3);
    REQUIRE(map.height() == 2);
    REQUIRE(map.pixelWidth() == 48.0F);
    REQUIRE(map.pixelHeight() == 32.0F);
    REQUIRE(map.tileAt({0, 0}) == 0);
    REQUIRE(map.tileAt({1, 0}) == 1);
    REQUIRE(map.tileAt({0, 1}) == 1);
    REQUIRE(map.tileAt({2, 1}) == 0);
}

TEST_CASE("Tile movement blocking comes from its definition", "[world][tile-map]")
{
    // A declared tile that blocks nothing: being a tile is not what blocks.
    const simple_platformer::TileMap map = tests::TileMapBuilder({"#x"}).where('x', tests::Tile());

    REQUIRE(map.blocksMovement({0, 0}));
    REQUIRE_FALSE(map.blocksMovement({1, 0}));
}

TEST_CASE("Map sides and bottom block movement while the top stays open", "[world][tile-map]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"..", ".."});

    REQUIRE(map.blocksMovement({-1, 0}));
    REQUIRE(map.blocksMovement({2, 0}));
    REQUIRE(map.blocksMovement({-1, -1}));
    REQUIRE(map.blocksMovement({2, -1}));
    REQUIRE(map.blocksMovement({0, 2}));
    REQUIRE_FALSE(map.blocksMovement({0, -1}));
    REQUIRE_FALSE(map.blocksMovement({0, 0}));
}

TEST_CASE("Tile lookup rejects positions outside the map", "[world][tile-map]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"..", ".."});

    REQUIRE_THROWS_AS(map.tileAt({-1, 0}), std::out_of_range);
    REQUIRE_THROWS_AS(map.tileAt({2, 0}), std::out_of_range);
    REQUIRE_THROWS_AS(map.tileAt({0, 2}), std::out_of_range);
}

TEST_CASE("ASCII tile maps reject malformed input", "[world][tile-map]")
{
    using simple_platformer::TileMap;
    const std::vector<simple_platformer::TileDefinition> definitions{{}};
    const std::map<char, int> legend{{'.', 0}};

    REQUIRE_THROWS_AS(
        TileMap::fromAscii(tests::TileSize, {}, definitions, legend), std::invalid_argument);
    REQUIRE_THROWS_AS(
        TileMap::fromAscii(tests::TileSize, {""}, definitions, legend), std::invalid_argument);
    REQUIRE_THROWS_AS(
        TileMap::fromAscii(tests::TileSize, {"..", "."}, definitions, legend),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        TileMap::fromAscii(tests::TileSize, {".x"}, definitions, legend), std::invalid_argument);
}

TEST_CASE("Breaking a tile replaces it with what its definition breaks into", "[world][tile-map]")
{
    simple_platformer::TileMap map =
        tests::TileMapBuilder({"g#"}).where('g', tests::Tile().blocksMovement().breaksInto('.'));

    REQUIRE(map.blocksMovement({0, 0}));
    REQUIRE(map.breakTile({0, 0}));
    REQUIRE(map.tileAt({0, 0}) == 0);
    REQUIRE_FALSE(map.blocksMovement({0, 0}));

    // Breaking it again finds an empty tile, which declares nothing to break into.
    REQUIRE_FALSE(map.breakTile({0, 0}));
    // The neighbouring solid tile declares nothing to break into either, so it stays.
    REQUIRE_FALSE(map.breakTile({1, 0}));
    REQUIRE(map.blocksMovement({1, 0}));
}

TEST_CASE("A tile map logs the cells it broke, in order", "[world][tile-map]")
{
    simple_platformer::TileMap map =
        tests::TileMapBuilder({"gg#"}).where('g', tests::Tile().blocksMovement().breaksInto('.'));
    REQUIRE(map.brokenCells().empty());

    REQUIRE(map.breakTile({1, 0}));
    REQUIRE(map.breakTile({0, 0}));
    // Neither an empty cell nor a solid one that declares nothing to break into is logged.
    REQUIRE_FALSE(map.breakTile({1, 0}));
    REQUIRE_FALSE(map.breakTile({2, 0}));
    REQUIRE(map.brokenCells() == std::vector<simple_platformer::GridPosition>{{1, 0}, {0, 0}});
}

TEST_CASE("Breaking reports failure outside the map instead of throwing", "[world][tile-map]")
{
    // Map boundaries block movement, so a cast can report a cell that is not in the map.
    simple_platformer::TileMap map = tests::TileMapBuilder({"..", ".."});

    REQUIRE_FALSE(map.breakTile({-1, 0}));
    REQUIRE_FALSE(map.breakTile({2, 0}));
    REQUIRE_FALSE(map.breakTile({0, 2}));
}

TEST_CASE("Tile maps reject invalid definitions and tile IDs", "[world][tile-map]")
{
    using simple_platformer::TileDefinition;
    using simple_platformer::TileMap;

    REQUIRE_THROWS_AS(
        TileMap(tests::TileSize, 0, 1, {}, {{false, false, {}}}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        TileMap(tests::TileSize, 2, 1, {0}, {{false, false, {}}}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        TileMap(tests::TileSize, 1, 1, {0}, {{true, true, {}}}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        TileMap(
            tests::TileSize,
            1,
            1,
            {2},
            std::vector<TileDefinition>{{false, false, {}}, {true, true, {}}}),
        std::invalid_argument);
}

TEST_CASE("A tile map contains the cells of its grid and no others", "[world][tile-map]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"...", "..."});
    REQUIRE(map.size().width == 3);
    REQUIRE(map.size().height == 2);
    REQUIRE(map.contains({0, 0}));
    REQUIRE(map.contains({2, 1}));
    REQUIRE_FALSE(map.contains({3, 1}));
    REQUIRE_FALSE(map.contains({2, 2}));
    REQUIRE_FALSE(map.contains({-1, 0}));
}

TEST_CASE("A tile map knows its tile size and measures itself by it", "[world][tile-map]")
{
    const simple_platformer::TileMap map(32, 3, 2, std::vector<int>(6, 0), {{false, false, {}}});

    REQUIRE(map.tileSize() == 32);
    REQUIRE(map.pixelWidth() == 96.0F);
    REQUIRE(map.pixelHeight() == 64.0F);
    REQUIRE_THROWS_AS(
        simple_platformer::TileMap(0, 1, 1, {0}, {{false, false, {}}}), std::invalid_argument);
}
