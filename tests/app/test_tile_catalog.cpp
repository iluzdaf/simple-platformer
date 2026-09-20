#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <nlohmann/json.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "content/tile_catalog.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/physics/segment_cast.hpp"

TEST_CASE("Tile catalogues reject unknown fields and identify their definitions", "[app][tiles]")
{
    auto root = nlohmann::json::parse(R"({"tiles":{
        "empty":{"blocksMovement":false,"blocksSight":false},
        "wall":{"blocksMovement":true,"blocksSight":true,"sprite":{"position":[0,0],"size":[16,16]}}
    }})");
    SECTION("Definition typo")
    {
        root["tiles"]["wall"]["blocksSighht"] = true;
    }
    SECTION("Sprite typo")
    {
        root["tiles"]["wall"]["sprite"]["width"] = 16;
    }
    SECTION("Invalid vector")
    {
        root["tiles"]["wall"]["sprite"]["position"] = {0};
    }
    REQUIRE_THROWS_WITH(
        simple_platformer::parseTileCatalog(root.dump(), "tiles.json"),
        Catch::Matchers::ContainsSubstring("tiles.json: tiles.wall"));
}

TEST_CASE("Tile legends resolve distinct movement and sight properties", "[app][tiles]")
{
    const auto catalog = simple_platformer::parseTileCatalog(
        R"({"tiles": {
        "empty": {"blocksMovement":false,"blocksSight":false},
        "glass": {"blocksMovement":true,"blocksSight":false,
                  "sprite":{"position": [16, 0], "size": [16, 16]}},
        "grass": {"blocksMovement":false,"blocksSight":true,
                  "sprite":{"position": [32, 0], "size": [16, 16]}}
    }})",
        "test tiles");
    const auto glass =
        simple_platformer::composeTileMap({".X."}, {{'.', "empty"}, {'X', "glass"}}, catalog);
    const auto grass =
        simple_platformer::composeTileMap({".G."}, {{'.', "empty"}, {'G', "grass"}}, catalog);
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
        simple_platformer::composeTileMap({"?"}, {{'.', "empty"}}, catalog), std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::composeTileMap({"X"}, {{'X', "missing"}}, catalog),
        std::invalid_argument);
}

TEST_CASE("Breakable tiles resolve breaksInto to a catalogue ID", "[app][tiles]")
{
    // cracked is declared after glass refers to it, so resolution cannot be a single pass.
    const auto catalog = simple_platformer::parseTileCatalog(
        R"({"tiles":{
        "empty":{"blocksMovement":false,"blocksSight":false},
        "glass":{"blocksMovement":true,"blocksSight":false,
                 "sprite":{"position":[0,0],"size":[16,16]},"breaksInto":"cracked"},
        "cracked":{"blocksMovement":true,"blocksSight":false,
                   "sprite":{"position":[16,0],"size":[16,16]},"breaksInto":"empty"},
        "stone":{"blocksMovement":true,"blocksSight":true,
                 "sprite":{"position":[32,0],"size":[16,16]}}}})",
        "tiles.json");

    const auto& glass = catalog.definitions[static_cast<std::size_t>(catalog.ids.at("glass"))];
    const auto& cracked = catalog.definitions[static_cast<std::size_t>(catalog.ids.at("cracked"))];
    const auto& stone = catalog.definitions[static_cast<std::size_t>(catalog.ids.at("stone"))];

    REQUIRE(glass.breaksIntoTileId == catalog.ids.at("cracked"));
    REQUIRE(cracked.breaksIntoTileId == catalog.ids.at("empty"));
    // A tile that says nothing about breaking is unbreakable.
    REQUIRE_FALSE(stone.breaksIntoTileId.has_value());
}

TEST_CASE("Tile catalogues reject unusable breaksInto targets", "[app][tiles]")
{
    const auto parse = [](const std::string& breaksInto)
    {
        return simple_platformer::parseTileCatalog(
            R"({"tiles":{
        "empty":{"blocksMovement":false,"blocksSight":false},
        "glass":{"blocksMovement":true,"blocksSight":false,
                 "sprite":{"position":[0,0],"size":[16,16]},"breaksInto":")" +
                breaksInto + R"("}}})",
            "tiles.json");
    };

    REQUIRE_NOTHROW(parse("empty"));
    REQUIRE_THROWS_WITH(
        parse("missing"),
        Catch::Matchers::ContainsSubstring(
            "tiles.json: tiles.glass.breaksInto: unknown tile name 'missing'"));
    // Breaking into itself would leave the tile in place forever.
    REQUIRE_THROWS_AS(parse("glass"), std::invalid_argument);
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
               "sprite":{"position": [0, 0], "size": [0, 16]}}}})",
            "test"),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::loadTileCatalog("missing-tiles.json"), std::invalid_argument);
}
