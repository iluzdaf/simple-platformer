#include <filesystem>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <nlohmann/json.hpp>

#include "content/level_data.hpp"

namespace
{
    nlohmann::json minimalLevel()
    {
        return nlohmann::json::parse(R"({
            "tileLegend": {".": "empty", "#": "stone"},
            "map": ["....", "####"],
            "playerSpawnCell": [0, 0],
            "actors": [],
            "pickups": [],
            "exit": {"definition": "door", "spawnCell": [3, 0]}
        })");
    }
}

TEST_CASE(
    "Level integer fields reject values outside their destination types",
    "[app][content][json]")
{
    auto level = minimalLevel();
    SECTION("Oversized cell")
    {
        level["playerSpawnCell"][0] = 4294967296LL;
    }
    SECTION("Undersized cell")
    {
        level["playerSpawnCell"][1] = -4294967296LL;
    }
    SECTION("Pickup quantity")
    {
        level["pickups"] = nlohmann::json::array(
            {{{"item", "key"},
              {"quantity", 4294967297LL},
              {"bodySize", {8, 8}},
              {"spawnCell", {1, 0}}}});
    }
    SECTION("Exit destination")
    {
        level["exit"]["nextLevel"] = 4294967297LL;
    }
    REQUIRE_THROWS_AS(
        simple_platformer::parseLevelData(level.dump(), "placements.json"), std::invalid_argument);
}

TEST_CASE("Explicit level placements reject unknown fields", "[app][content][json]")
{
    auto level = minimalLevel();
    SECTION("Root")
    {
        level["actorrs"] = nlohmann::json::array();
    }
    SECTION("Actor")
    {
        level["actors"] = nlohmann::json::array(
            {{{"definition", "guard"},
              {"spawnCell", {1, 0}},
              {"patroll", nlohmann::json::object()}}});
    }
    SECTION("Patrol")
    {
        level["actors"] = nlohmann::json::array(
            {{{"definition", "guard"},
              {"spawnCell", {1, 0}},
              {"patrol", {{"firstCell", {0, 0}}, {"secondCell", {1, 0}}, {"speeed", 1}}}}});
    }
    SECTION("Pickup")
    {
        level["pickups"] = nlohmann::json::array(
            {{{"item", "key"},
              {"quantity", 1},
              {"bodySize", {8, 8}},
              {"spawnCell", {2, 0}},
              {"quantitty", 3}}});
    }
    SECTION("Exit")
    {
        level["exit"]["nextLevell"] = 2;
    }
    SECTION("Exit requirement")
    {
        level["exit"]["requirement"] = {{"item", "key"}, {"quantity", 1}, {"consumme", true}};
    }
    REQUIRE_THROWS_AS(
        simple_platformer::parseLevelData(level.dump(), "placements.json"), std::invalid_argument);
}

TEST_CASE("Tile legend keys must be one character", "[app][content][json]")
{
    auto level = minimalLevel();
    level["tileLegend"]["long"] = "grass";

    REQUIRE_THROWS_AS(
        simple_platformer::parseLevelData(level.dump(), "bad legend"), std::invalid_argument);
}

TEST_CASE("Map symbols must be declared in a legend", "[app][content][json]")
{
    auto level = minimalLevel();
    level["map"][0] = ".X..";

    REQUIRE_THROWS_AS(
        simple_platformer::parseLevelData(level.dump(), "bad symbol"), std::invalid_argument);
}

TEST_CASE("Level maps require rectangular rows", "[app][content][json]")
{
    auto level = minimalLevel();
    level["map"][1] = "###";

    REQUIRE_THROWS_AS(
        simple_platformer::parseLevelData(level.dump(), "ragged level"), std::invalid_argument);
}

TEST_CASE("Actor placements need a non-empty definition", "[app][content][json]")
{
    auto level = minimalLevel();
    level["actors"] = nlohmann::json::array({{{"definition", ""}, {"spawnCell", {1, 0}}}});

    REQUIRE_THROWS_AS(
        simple_platformer::parseLevelData(level.dump(), "empty actor name"), std::invalid_argument);
}

TEST_CASE("Actor placements choose a cell or feet, not both", "[app][content][json]")
{
    auto level = minimalLevel();
    level["actors"] = nlohmann::json::array(
        {{{"definition", "guard"}, {"spawnCell", {1, 0}}, {"spawnFeet", {24, 16}}}});

    REQUIRE_THROWS_AS(
        simple_platformer::parseLevelData(level.dump(), "ambiguous actor"), std::invalid_argument);
}

TEST_CASE("Pickup definitions cannot mix with inline stack fields", "[app][content][json]")
{
    auto level = minimalLevel();
    level["pickups"] = nlohmann::json::array({{{"definition", "treasure"}, {"spawnCell", {1, 0}}}});
    SECTION("Item")
    {
        level["pickups"][0]["item"] = "key";
    }
    SECTION("Quantity")
    {
        level["pickups"][0]["quantity"] = 2;
    }
    REQUIRE_THROWS_AS(
        simple_platformer::parseLevelData(level.dump(), "placement.json"), std::invalid_argument);
}

TEST_CASE("Pickup definition names cannot be empty", "[app][content][json]")
{
    auto level = minimalLevel();
    level["pickups"] = nlohmann::json::array({{{"definition", ""}, {"spawnCell", {1, 0}}}});

    REQUIRE_THROWS_AS(
        simple_platformer::parseLevelData(level.dump(), "placement.json"), std::invalid_argument);
}

TEST_CASE("Inline pickup placements require a body size", "[app][content][json]")
{
    auto level = minimalLevel();
    level["pickups"] =
        nlohmann::json::array({{{"item", "key"}, {"quantity", 1}, {"spawnCell", {1, 0}}}});

    REQUIRE_THROWS_WITH(
        simple_platformer::parseLevelData(level.dump(), "placement.json"),
        Catch::Matchers::ContainsSubstring("bodySize"));
}

TEST_CASE("Missing level JSON is rejected at the file boundary", "[app][content][json]")
{
    REQUIRE_THROWS_AS(
        simple_platformer::loadLevelData(std::filesystem::path("assets/does_not_exist.json")),
        std::invalid_argument);
}

TEST_CASE("Level JSON requires a tile legend", "[app][content][json]")
{
    auto level = minimalLevel();
    level.erase("tileLegend");

    REQUIRE_THROWS_WITH(
        simple_platformer::parseLevelData(level.dump(), "no legend"),
        Catch::Matchers::ContainsSubstring("tileLegend"));
}
