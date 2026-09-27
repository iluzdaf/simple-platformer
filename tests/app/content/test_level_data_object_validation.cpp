#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
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

TEST_CASE("Object markers require exactly one player and exit", "[app][content][json]")
{
    auto level = markerLevel();
    SECTION("Repeated player")
    {
        level["map"] = {"PPE"};
    }
    SECTION("Repeated exit")
    {
        level["map"] = {"PEE"};
    }
    SECTION("Missing player")
    {
        level["map"] = {".E"};
    }
    SECTION("Missing exit")
    {
        level["map"] = {"P."};
    }
    REQUIRE_THROWS_AS(
        simple_platformer::parseLevelData(level.dump(), "bad markers"), std::invalid_argument);
}

TEST_CASE("Object markers cannot duplicate explicit placements", "[app][content][json]")
{
    auto level = markerLevel();
    SECTION("Player")
    {
        level["playerSpawnCell"] = {0, 0};
    }
    SECTION("Exit")
    {
        level["exit"] = {{"definition", "test_door"}, {"spawnCell", {1, 0}}};
    }
    REQUIRE_THROWS_AS(
        simple_platformer::parseLevelData(level.dump(), "bad markers"), std::invalid_argument);
}

TEST_CASE("Object legend symbols are single glyphs distinct from terrain", "[app][content][json]")
{
    auto level = markerLevel();
    SECTION("Terrain symbol")
    {
        level["objectLegend"]["#"] = {{"type", "actor"}, {"definition", "zombie"}};
    }
    SECTION("Long symbol")
    {
        level["objectLegend"]["ZZ"] = {{"type", "actor"}, {"definition", "zombie"}};
    }
    REQUIRE_THROWS_AS(
        simple_platformer::parseLevelData(level.dump(), "bad markers"), std::invalid_argument);
}

TEST_CASE(
    "Unused object templates still need a valid category and definition",
    "[app][content][json]")
{
    auto level = markerLevel();
    SECTION("Empty actor definition")
    {
        level["objectLegend"]["Z"] = {{"type", "actor"}, {"definition", ""}};
    }
    SECTION("Missing actor definition")
    {
        level["objectLegend"]["Z"] = {{"type", "actor"}};
    }
    SECTION("Unknown category")
    {
        level["objectLegend"]["Z"] = {{"type", "zombie"}, {"definition", "zombie"}};
    }
    REQUIRE_THROWS_AS(
        simple_platformer::parseLevelData(level.dump(), "bad markers"), std::invalid_argument);
}

TEST_CASE("Object templates cannot provide a placement", "[app][content][json]")
{
    auto level = markerLevel();
    level["objectLegend"]["P"]["spawnFeet"] = {1, 2};

    REQUIRE_THROWS_AS(
        simple_platformer::parseLevelData(level.dump(), "bad markers"), std::invalid_argument);
}

TEST_CASE("Pickup object templates require a positive quantity", "[app][content][json]")
{
    auto level = markerLevel();
    level["objectLegend"]["K"] = {
        {"type", "pickup"}, {"item", "key"}, {"quantity", 0}, {"bodySize", {8, 8}}};

    REQUIRE_THROWS_AS(
        simple_platformer::parseLevelData(level.dump(), "bad markers"), std::invalid_argument);
}

TEST_CASE("Unused object templates reject unknown fields", "[app][content][json]")
{
    auto level = markerLevel();
    SECTION("Actor")
    {
        level["objectLegend"]["Z"] = {
            {"type", "actor"}, {"definition", "guard"}, {"patroll", true}};
    }
    SECTION("Player")
    {
        level["objectLegend"]["Q"] = {{"type", "player"}, {"health", 4}};
    }
    SECTION("Pickup")
    {
        level["objectLegend"]["K"] = {{"type", "pickup"}, {"definition", "key"}, {"quantitty", 3}};
    }
    SECTION("Exit")
    {
        level["objectLegend"]["X"] = {{"type", "exit"}, {"definition", "door"}, {"nextLevell", 2}};
    }
    REQUIRE_THROWS_AS(
        simple_platformer::parseLevelData(level.dump(), "bad markers"), std::invalid_argument);
}
