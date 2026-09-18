#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "game/tile_catalog.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/physics/segment_cast.hpp"

TEST_CASE("Tile legends resolve distinct movement and sight properties", "[app][tiles]")
{
    const auto catalog = simple_platformer::parseTileCatalog(
        R"({"tiles": {
        "empty": {"blocksMovement":false,"blocksSight":false},
        "glass": {"blocksMovement":true,"blocksSight":false,
                  "sprite":{"x":16,"y":0,"width":16,"height":16}},
        "grass": {"blocksMovement":false,"blocksSight":true,
                  "sprite":{"x":32,"y":0,"width":16,"height":16}}
    }})",
        "test tiles");
    const auto glass =
        simple_platformer::makeTileMap({".X."}, {{'.', "empty"}, {'X', "glass"}}, catalog);
    const auto grass =
        simple_platformer::makeTileMap({".G."}, {{'.', "empty"}, {'G', "grass"}}, catalog);
    REQUIRE(glass.blocksMovement({1, 0}));
    REQUIRE_FALSE(glass.blocksSight({1, 0}));
    REQUIRE_FALSE(grass.blocksMovement({1, 0}));
    REQUIRE(grass.blocksSight({1, 0}));
    REQUIRE(glass.definitionAt({1, 0}).sprite.position.x == 16);
    REQUIRE(grass.definitionAt({1, 0}).sprite.position.x == 32);
    const simple_platformer::NpcSenses senses;
    REQUIRE(simple_platformer::canSeeTarget(glass, {{2, 2}, {4, 4}}, {{36, 2}, {4, 4}}, senses));
    REQUIRE_FALSE(
        simple_platformer::canSeeTarget(grass, {{2, 2}, {4, 4}}, {{36, 2}, {4, 4}}, senses));
    REQUIRE(
        simple_platformer::segmentCastMovementBlockingTiles(glass, {4, 4}, {38, 4}).has_value());
    REQUIRE_FALSE(
        simple_platformer::segmentCastMovementBlockingTiles(grass, {4, 4}, {38, 4}).has_value());
    REQUIRE_THROWS_AS(
        simple_platformer::makeTileMap({"?"}, {{'.', "empty"}}, catalog), std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::makeTileMap({"X"}, {{'X', "missing"}}, catalog), std::invalid_argument);
}

TEST_CASE("Tile catalogs reject missing empty tiles and malformed definitions", "[app][tiles]")
{
    REQUIRE_THROWS_AS(
        simple_platformer::parseTileCatalog(R"({"tiles":{}})", "test"), std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::parseTileCatalog(
            R"({"tiles":{
        "empty":{"blocksMovement":true,"blocksSight":false}}})",
            "test"),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::parseTileCatalog(
            R"({"tiles":{
        "empty":{"blocksMovement":false,"blocksSight":false},
        "bad":{"blocksMovement":true,"blocksSight":true,
               "sprite":{"x":0,"y":0,"width":0,"height":16}}}})",
            "test"),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::loadTileCatalog("missing-tiles.json"), std::invalid_argument);
}
