#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <vector>

#include "simple_platformer/world/tile_map.hpp"

TEST_CASE("An ASCII tile map is rectangular and row-major", "[world][tile-map]")
{
    const simple_platformer::TileMap map = simple_platformer::TileMap::fromAscii({".#.", "##."});

    REQUIRE(map.width() == 3);
    REQUIRE(map.height() == 2);
    REQUIRE(map.pixelWidth() == 48.0F);
    REQUIRE(map.pixelHeight() == 32.0F);
    REQUIRE(map.tileAt({0, 0}) == 0);
    REQUIRE(map.tileAt({1, 0}) == 1);
    REQUIRE(map.tileAt({0, 1}) == 1);
    REQUIRE(map.tileAt({2, 1}) == 0);
}

TEST_CASE("Tile solidity comes from its definition", "[world][tile-map]")
{
    const simple_platformer::TileMap map(2, 1, {1, 2}, {{false}, {true}, {false}});

    REQUIRE(map.isSolid({0, 0}));
    REQUIRE_FALSE(map.isSolid({1, 0}));
}

TEST_CASE("Map sides and bottom block movement while the top stays open", "[world][tile-map]")
{
    const simple_platformer::TileMap map = simple_platformer::TileMap::fromAscii({"..", ".."});

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
    const simple_platformer::TileMap map = simple_platformer::TileMap::fromAscii({"..", ".."});

    REQUIRE_THROWS_AS(map.tileAt({-1, 0}), std::out_of_range);
    REQUIRE_THROWS_AS(map.tileAt({2, 0}), std::out_of_range);
    REQUIRE_THROWS_AS(map.tileAt({0, 2}), std::out_of_range);
}

TEST_CASE("ASCII tile maps reject malformed input", "[world][tile-map]")
{
    REQUIRE_THROWS_AS(simple_platformer::TileMap::fromAscii({}), std::invalid_argument);
    REQUIRE_THROWS_AS(simple_platformer::TileMap::fromAscii({""}), std::invalid_argument);
    REQUIRE_THROWS_AS(simple_platformer::TileMap::fromAscii({"..", "."}), std::invalid_argument);
    REQUIRE_THROWS_AS(simple_platformer::TileMap::fromAscii({".x"}), std::invalid_argument);
}

TEST_CASE("Tile maps reject invalid definitions and tile IDs", "[world][tile-map]")
{
    using simple_platformer::TileDefinition;
    using simple_platformer::TileMap;

    REQUIRE_THROWS_AS(TileMap(0, 1, {}, {{false}}), std::invalid_argument);
    REQUIRE_THROWS_AS(TileMap(2, 1, {0}, {{false}}), std::invalid_argument);
    REQUIRE_THROWS_AS(TileMap(1, 1, {0}, {{true}}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        TileMap(1, 1, {2}, std::vector<TileDefinition>{{false}, {true}}), std::invalid_argument);
}
