#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <glm/vec2.hpp>
#include <nlohmann/json.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "content/tile_catalog.hpp"
#include "simple_platformer/physics/segment_cast.hpp"
#include "simple_platformer/world/sight.hpp"

TEST_CASE("Tile catalogs reject unknown fields and identify their definitions", "[app][tiles]")
{
    auto tileJson = nlohmann::json::parse(R"({"tileSize":16,"tiles":{
        "empty":{"blocksMovement":false,"blocksSight":false},
        "wall":{"blocksMovement":true,"blocksSight":true,"sprite":{"position":[0,0]}}
    }})");
    SECTION("Definition typo")
    {
        tileJson["tiles"]["wall"]["blocksSighht"] = true;
    }
    SECTION("Sprite typo")
    {
        tileJson["tiles"]["wall"]["sprite"]["width"] = 16;
    }
    SECTION("Invalid vector")
    {
        tileJson["tiles"]["wall"]["sprite"]["position"] = {0};
    }
    REQUIRE_THROWS_WITH(
        simple_platformer::parseTileCatalog(tileJson.dump(), "tiles.json"),
        Catch::Matchers::ContainsSubstring("tiles.json: tiles.wall"));
}

TEST_CASE("Tile legends resolve distinct movement and sight properties", "[app][tiles]")
{
    const auto catalog = simple_platformer::parseTileCatalog(
        R"({"tileSize":16,"tiles": {
        "empty": {"blocksMovement":false,"blocksSight":false},
        "glass": {"blocksMovement":true,"blocksSight":false,
                  "sprite":{"position": [16, 0]}},
        "grass": {"blocksMovement":false,"blocksSight":true,
                  "sprite":{"position": [32, 0]}}
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
    REQUIRE(simple_platformer::lineOfSight(glass, {4.0F, 4.0F}, {38.0F, 4.0F}));
    REQUIRE_FALSE(simple_platformer::lineOfSight(grass, {4.0F, 4.0F}, {38.0F, 4.0F}));
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

TEST_CASE("Tile definitions mark climbable solid surfaces explicitly", "[app][tiles]")
{
    const auto catalog = simple_platformer::parseTileCatalog(
        R"({"tileSize":16,"tiles":{
        "empty":{"blocksMovement":false,"blocksSight":false},
        "stone":{"blocksMovement":true,"blocksSight":true,"climbable":true,
                 "sprite":{"position":[0,0]}},
        "glass":{"blocksMovement":true,"blocksSight":false,
                 "sprite":{"position":[16,0]}}
    }})",
        "tiles.json");
    const auto map = simple_platformer::composeTileMap(
        {".SG"}, {{'.', "empty"}, {'S', "stone"}, {'G', "glass"}}, catalog);

    REQUIRE(map.climbableAt({1, 0}));
    REQUIRE_FALSE(map.climbableAt({2, 0}));
    REQUIRE_FALSE(map.climbableAt({-1, 0}));
    REQUIRE_FALSE(map.climbableAt({0, 0}));
}

TEST_CASE("Only movement-blocking tiles can be marked climbable", "[app][tiles]")
{
    REQUIRE_THROWS_WITH(
        simple_platformer::parseTileCatalog(
            R"({"tileSize":16,"tiles":{
        "empty":{"blocksMovement":false,"blocksSight":false},
        "grass":{"blocksMovement":false,"blocksSight":true,"climbable":true,
                 "sprite":{"position":[0,0]}}
    }})",
            "tiles.json"),
        Catch::Matchers::ContainsSubstring("climbable tile 'grass' must block movement"));
}

TEST_CASE("Breakable tiles resolve breaksInto to a catalog ID", "[app][tiles]")
{
    // cracked is declared after glass refers to it, so resolution cannot be a single pass.
    const auto catalog = simple_platformer::parseTileCatalog(
        R"({"tileSize":16,"tiles":{
        "empty":{"blocksMovement":false,"blocksSight":false},
        "glass":{"blocksMovement":true,"blocksSight":false,
                 "sprite":{"position":[0,0]},"breaksInto":"cracked"},
        "cracked":{"blocksMovement":true,"blocksSight":false,
                   "sprite":{"position":[16,0]},"breaksInto":"empty"},
        "stone":{"blocksMovement":true,"blocksSight":true,
                 "sprite":{"position":[32,0]}}}})",
        "tiles.json");

    const auto& glass = catalog.definitions[static_cast<std::size_t>(catalog.ids.at("glass"))];
    const auto& cracked = catalog.definitions[static_cast<std::size_t>(catalog.ids.at("cracked"))];
    const auto& stone = catalog.definitions[static_cast<std::size_t>(catalog.ids.at("stone"))];

    REQUIRE(glass.breaksIntoTileId == catalog.ids.at("cracked"));
    REQUIRE(cracked.breaksIntoTileId == catalog.ids.at("empty"));
    REQUIRE_FALSE(stone.breaksIntoTileId.has_value());
}

TEST_CASE("Tile catalogs reject unusable breaksInto targets", "[app][tiles]")
{
    const auto parse = [](const std::string& breaksInto)
    {
        return simple_platformer::parseTileCatalog(
            R"({"tileSize":16,"tiles":{
        "empty":{"blocksMovement":false,"blocksSight":false},
        "glass":{"blocksMovement":true,"blocksSight":false,
                 "sprite":{"position":[0,0]},"breaksInto":")" +
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
        simple_platformer::parseTileCatalog(R"({"tileSize":16,"tiles":{}})", "test"),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::parseTileCatalog(
            R"({"tileSize":16,"tiles":{
        "empty":{"blocksMovement":true,"blocksSight":false}}})",
            "test"),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::parseTileCatalog(
            R"({"tileSize":16,"tiles":{
        "empty":{"blocksMovement":false,"blocksSight":false},
        "bad":{"blocksMovement":true,"blocksSight":true,
               "sprite":{"position": [0, -16]}}}})",
            "test"),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::loadTileCatalog("missing-tiles.json"), std::invalid_argument);
}

TEST_CASE("A tile catalog declares its tile size and maps compose at it", "[app][tiles]")
{
    const auto catalog = simple_platformer::parseTileCatalog(
        R"({"tileSize":32,"tiles":{
        "empty":{"blocksMovement":false,"blocksSight":false},
        "wall":{"blocksMovement":true,"blocksSight":true,"sprite":{"position":[0,0]}}
    }})",
        "tiles.json");
    const auto map =
        simple_platformer::composeTileMap({".W"}, {{'.', "empty"}, {'W', "wall"}}, catalog);

    REQUIRE(catalog.tileSize == 32);
    REQUIRE(map.tileSize() == 32);
    REQUIRE(map.pixelWidth() == 64.0F);
    REQUIRE(map.definitionAt({1, 0}).sprite.size == glm::vec2{32.0F, 32.0F});
}

TEST_CASE("Tile catalogs require a positive tile size", "[app][tiles]")
{
    REQUIRE_THROWS_WITH(
        simple_platformer::parseTileCatalog(
            R"({"tiles":{"empty":{"blocksMovement":false,"blocksSight":false}}})", "tiles.json"),
        Catch::Matchers::ContainsSubstring("tileSize"));
    REQUIRE_THROWS_AS(
        simple_platformer::parseTileCatalog(
            R"({"tileSize":0,"tiles":{"empty":{"blocksMovement":false,"blocksSight":false}}})",
            "tiles.json"),
        std::invalid_argument);
}

TEST_CASE("A tile sprite gives only where it starts, since it is one tile", "[app][tiles]")
{
    REQUIRE_THROWS_WITH(
        simple_platformer::parseTileCatalog(
            R"({"tileSize":16,"tiles":{
        "empty":{"blocksMovement":false,"blocksSight":false},
        "wide":{"blocksMovement":true,"blocksSight":true,"sprite":{"position":[0,0],"size":[32,16]}}
    }})",
            "tiles.json"),
        Catch::Matchers::ContainsSubstring("tiles.wide.sprite"));
}
