#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <nlohmann/json.hpp>

#include "content/level_data.hpp"

namespace
{
    nlohmann::json markerLevel()
    {
        return nlohmann::json::parse(R"({
            "tileLegend": {".": "empty", "#": "stone"},
            "objectLegend": {
                "P": {"type": "player"},
                "E": {"type": "exit", "definition": "test_door"}
            },
            "map": ["PE"]
        })");
    }
}

TEST_CASE("Map diagnostics identify authored cells and row widths", "[app][content][json]")
{
    auto level = markerLevel();
    SECTION("Unknown symbol")
    {
        level["map"] = {"P?E"};
        REQUIRE_THROWS_WITH(
            simple_platformer::parseLevelData(level.dump(), "level.json"),
            "level.json: map[0][1]: unknown symbol '?'; define it in tileLegend or objectLegend");
    }
    SECTION("Row width")
    {
        level["map"] = {"PE", "."};
        REQUIRE_THROWS_WITH(
            simple_platformer::parseLevelData(level.dump(), "level.json"),
            "level.json: map[1]: expected 2 columns, got 1");
    }
}

TEST_CASE("Duplicate-marker diagnostics identify both placements", "[app][content][json]")
{
    auto level = markerLevel();
    SECTION("Repeated player marker")
    {
        level["map"] = {"PPE"};
        REQUIRE_THROWS_WITH(
            simple_platformer::parseLevelData(level.dump(), "level.json"),
            "level.json: map[0][1]: second player marker 'P'; player already placed at map[0][0]");
    }
    SECTION("Marker conflicts with an explicit exit")
    {
        level["exit"] = {{"definition", "test_door"}, {"spawnCell", {1, 0}}};
        REQUIRE_THROWS_WITH(
            simple_platformer::parseLevelData(level.dump(), "level.json"),
            "level.json: map[0][1]: second exit marker 'E'; exit already placed at exit");
    }
}

TEST_CASE("Object-template diagnostics name authored legend fields", "[app][content][json]")
{
    auto level = markerLevel();
    SECTION("Invalid pickup quantity")
    {
        level["objectLegend"]["K"] = {
            {"type", "pickup"}, {"item", "key"}, {"quantity", 0}, {"bodySize", {8, 8}}};
        REQUIRE_THROWS_WITH(
            simple_platformer::parseLevelData(level.dump(), "level.json"),
            "level.json: objectLegend.K.quantity: expected a positive integer, got 0");
    }
    SECTION("Invalid exit setting")
    {
        level["objectLegend"]["E"]["consumeItem"] = 1;
        REQUIRE_THROWS_WITH(
            simple_platformer::parseLevelData(level.dump(), "level.json"),
            "level.json: objectLegend.E.consumeItem: expected true or false");
    }
    SECTION("Missing exit definition")
    {
        level["objectLegend"]["E"].erase("definition");
        REQUIRE_THROWS_WITH(
            simple_platformer::parseLevelData(level.dump(), "level.json"),
            "level.json: objectLegend.E: missing 'definition'");
    }
    SECTION("Empty exit definition")
    {
        level["objectLegend"]["E"]["definition"] = "";
        REQUIRE_THROWS_WITH(
            simple_platformer::parseLevelData(level.dump(), "level.json"),
            "level.json: objectLegend.E.definition: exit definition name cannot be empty");
    }
}

TEST_CASE("Explicit-placement diagnostics retain array paths", "[app][content][json]")
{
    auto level = markerLevel();
    level["actors"] = nlohmann::json::array({{{"definition", ""}, {"spawnCell", {0, 0}}}});

    REQUIRE_THROWS_WITH(
        simple_platformer::parseLevelData(level.dump(), "level.json"),
        "level.json: actors[0].definition: actor definition name cannot be empty");
}

TEST_CASE("JSON syntax diagnostics include one-based line and byte column", "[app][content][json]")
{
    REQUIRE_THROWS_WITH(
        simple_platformer::parseLevelData("{\n  ?\n}", "broken.json"),
        Catch::Matchers::ContainsSubstring("broken.json: line 2, column 3: invalid JSON:"));
    REQUIRE_THROWS_WITH(
        simple_platformer::parseLevelData("{\n", "unfinished.json"),
        Catch::Matchers::ContainsSubstring("unfinished.json: line 2, column 1: invalid JSON:"));
}
