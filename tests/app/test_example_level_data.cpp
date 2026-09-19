#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <stdexcept>
#include <nlohmann/json.hpp>

#include "game/example_level_data.hpp"
#include "simple_platformer/npc/npc.hpp"

TEST_CASE("Level diagnostics identify authored fields and map cells", "[app][content][json]")
{
    auto level = nlohmann::json::parse(R"({
        "objectLegend":{"P":{"type":"player"},"E":{"type":"exit"}},
        "map":["PE"]
    })");
    SECTION("Unknown map symbol includes row column and symbol")
    {
        level["map"] = {"P?E"};
        REQUIRE_THROWS_WITH(
            simple_platformer::parseExampleLevelData(level.dump(), "level.json"),
            "level.json: map[0][1]: unknown symbol '?'; define it in tileLegend or objectLegend");
    }
    SECTION("Repeated marker identifies both placements")
    {
        level["map"] = {"PPE"};
        REQUIRE_THROWS_WITH(
            simple_platformer::parseExampleLevelData(level.dump(), "level.json"),
            "level.json: map[0][1]: second player marker 'P'; player already placed at map[0][0]");
    }
    SECTION("Exit conflict identifies explicit placement")
    {
        level["exit"] = {{"spawnCell", {1, 0}}};
        REQUIRE_THROWS_WITH(
            simple_platformer::parseExampleLevelData(level.dump(), "level.json"),
            "level.json: map[0][1]: second exit marker 'E'; exit already placed at exit");
    }
    SECTION("Legend errors do not report generated array indices")
    {
        level["objectLegend"]["K"] = {{"type", "pickup"}, {"item", "key"}, {"quantity", 0}};
        REQUIRE_THROWS_WITH(
            simple_platformer::parseExampleLevelData(level.dump(), "level.json"),
            "level.json: objectLegend.K.quantity: expected a positive integer, got 0");
    }
    SECTION("Exit settings identify the legend field")
    {
        level["objectLegend"]["E"]["consumeItem"] = 1;
        REQUIRE_THROWS_WITH(
            simple_platformer::parseExampleLevelData(level.dump(), "level.json"),
            "level.json: objectLegend.E.consumeItem: expected true or false");
    }
    SECTION("Row width includes expected and actual widths")
    {
        level["map"] = {"PE", "."};
        REQUIRE_THROWS_WITH(
            simple_platformer::parseExampleLevelData(level.dump(), "level.json"),
            "level.json: map[1]: expected 2 columns, got 1");
    }
    SECTION("Explicit entries retain their array paths")
    {
        level["actors"] = nlohmann::json::array({{{"definition", ""}, {"spawnCell", {0, 0}}}});
        REQUIRE_THROWS_WITH(
            simple_platformer::parseExampleLevelData(level.dump(), "level.json"),
            "level.json: actors[0].definition: actor definition name cannot be empty");
    }
}

TEST_CASE("JSON syntax diagnostics include one-based line and byte column", "[app][content][json]")
{
    REQUIRE_THROWS_WITH(
        simple_platformer::parseExampleLevelData("{\n  ?\n}", "broken.json"),
        Catch::Matchers::ContainsSubstring("broken.json: line 2, column 3: invalid JSON:"));
    REQUIRE_THROWS_WITH(
        simple_platformer::parseExampleLevelData("{\n", "unfinished.json"),
        Catch::Matchers::ContainsSubstring("unfinished.json: line 2, column 1: invalid JSON:"));
}

TEST_CASE(
    "Object markers expand into ordinary placements over empty terrain",
    "[app][content][json]")
{
    const auto data = simple_platformer::parseExampleLevelData(
        R"({
        "tileLegend":{".":"empty", "#":"stone", "G":"grass"},
        "objectLegend":{
            "P":{"type":"player"},
            "Z":{"type":"actor", "definition":"zombie", "patrol":{"firstCell":[1,0],"secondCell":[2,0]}},
            "B":{"type":"actor", "definition":"bat"}, "S":{"type":"actor", "definition":"zombie_soldier"},
            "K":{"type":"pickup","item":"key","quantity":2},
            "E":{"type":"exit","requirement":{"item":"key","quantity":1},
                 "consumeItem":true,"nextLevel":2}
        },
        "map":["PZZBSKKEG", "#########"],
        "actors":[{"definition":"zombie","spawnCell":[8,0]}],
        "pickups":[{"item":"coin","quantity":3,"spawnFeet":[136,8]}]
    })",
        "markers");
    REQUIRE(data.playerSpawnFeet.x == 8);
    REQUIRE(data.playerSpawnFeet.y == 16);
    REQUIRE(data.actors.size() == 5);
    REQUIRE(data.actors[0].spawnFeet.x == 136);
    REQUIRE(data.actors[1].spawnFeet.x == 24);
    REQUIRE(data.actors[2].spawnFeet.x == 40);
    REQUIRE(data.actors[1].patrol.has_value());
    REQUIRE(data.actors[3].definitionName == "bat");
    REQUIRE(data.actors[4].definitionName == "zombie_soldier");
    REQUIRE(data.pickups.size() == 3);
    REQUIRE(data.pickups[1].stack.item == "key");
    REQUIRE(data.pickups[1].stack.quantity == 2);
    REQUIRE(data.pickups[2].spawnFeet.x == 104);
    REQUIRE(data.exit.spawnFeet.x == 120);
    REQUIRE(data.exit.requirement.has_value());
    REQUIRE(data.exit.consumeItem);
    REQUIRE(data.exit.nextLevel == 2);
    REQUIRE(data.tileLegend.at('Z') == "empty");
    REQUIRE(data.tileLegend.at('G') == "grass");
}

TEST_CASE("Object legends reject ambiguous or invalid placements", "[app][content][json]")
{
    auto level = nlohmann::json::parse(R"({
        "objectLegend":{"P":{"type":"player"},"E":{"type":"exit"}},
        "map":["PE"]
    })");
    SECTION("Repeated player")
    {
        level["map"] = {"PPE"};
    }
    SECTION("Repeated exit")
    {
        level["map"] = {"PEE"};
    }
    SECTION("Explicit player and marker")
    {
        level["playerSpawnCell"] = {0, 0};
    }
    SECTION("Explicit exit and marker")
    {
        level["exit"] = {{"spawnCell", {1, 0}}};
    }
    SECTION("Missing player")
    {
        level["map"] = {".E"};
    }
    SECTION("Missing exit")
    {
        level["map"] = {"P."};
    }
    SECTION("Shared symbol")
    {
        level["objectLegend"]["#"] = {{"type", "actor"}, {"definition", "zombie"}};
    }
    SECTION("Long symbol")
    {
        level["objectLegend"]["ZZ"] = {{"type", "actor"}, {"definition", "zombie"}};
    }
    SECTION("Empty definition name even when unused")
    {
        level["objectLegend"]["Z"] = {{"type", "actor"}, {"definition", ""}};
    }
    SECTION("Missing actor definition even when unused")
    {
        level["objectLegend"]["Z"] = {{"type", "actor"}};
    }
    SECTION("Unknown object category even when unused")
    {
        level["objectLegend"]["Z"] = {{"type", "zombie"}, {"definition", "zombie"}};
    }
    SECTION("Position in template")
    {
        level["objectLegend"]["P"]["spawnFeet"] = {1, 2};
    }
    SECTION("Invalid pickup quantity")
    {
        level["objectLegend"]["K"] = {{"type", "pickup"}, {"item", "key"}, {"quantity", 0}};
    }
    REQUIRE_THROWS_AS(
        simple_platformer::parseExampleLevelData(level.dump(), "bad markers"),
        std::invalid_argument);
}

TEST_CASE("Object-only levels may omit explicit placement arrays", "[app][content][json]")
{
    const auto data = simple_platformer::parseExampleLevelData(
        R"({
        "objectLegend":{"P":{"type":"player"},"E":{"type":"exit"}},
        "map":["PE"]
    })",
        "markers");
    REQUIRE(data.actors.empty());
    REQUIRE(data.pickups.empty());
    REQUIRE(data.exit.spawnFeet.x == 24);
}

TEST_CASE("Level JSON accepts a custom tile legend", "[app][content][json]")
{
    const auto data = simple_platformer::parseExampleLevelData(
        R"({
        "tileLegend": {".":"empty", "G":"grass", "X":"glass"},
        "map":[".GX"], "playerSpawnCell":[0,0], "actors":[], "pickups":[],
        "exit":{"spawnCell":[2,0]}
    })",
        "custom level");
    REQUIRE(data.tileLegend.at('G') == "grass");
    REQUIRE(data.mapRows.front() == ".GX");
    REQUIRE_THROWS_AS(
        simple_platformer::parseExampleLevelData(
            R"({
        "tileLegend":{"long":"grass"}, "map":["."]
    })",
            "bad legend"),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::parseExampleLevelData(
            R"({
        "tileLegend":{".":"empty"}, "map":["X"]
    })",
            "bad symbol"),
        std::invalid_argument);
}

TEST_CASE(
    "Level JSON describes placements without defining actor behaviour",
    "[app][content][json]")
{
    const auto data = simple_platformer::parseExampleLevelData(
        R"({
            "map": ["....", "####"],
            "playerSpawnCell": [1, 0],
            "actors": [
                {
                    "definition": "zombie",
                    "spawnCell": [2, 0],
                    "patrol": {
                        "firstCell": [2, 0],
                        "secondCell": [3, 0]
                    }
                },
                {
                    "definition": "bat",
                    "spawnFeet": [17, 9],
                    "patrol": {
                        "firstFeet": [17, 9],
                        "secondFeet": [25, 13]
                    }
                }
            ],
            "pickups": [{
                "item": "key",
                "quantity": 1,
                "spawnCell": [1, 0]
            }],
            "exit": {
                "spawnCell": [2, 0],
                "requirement": {"item": "key", "quantity": 1}
            }
        })",
        "test level");

    REQUIRE(data.mapRows.size() == 2);
    REQUIRE(data.playerSpawnFeet.x == 24.0F);
    REQUIRE(data.actors.size() == 2);
    REQUIRE(data.actors.front().definitionName == "zombie");
    REQUIRE(data.actors.front().spawnFeet.x == 40.0F);
    REQUIRE(data.actors.front().patrol.has_value());
    const simple_platformer::Patrol patrol =
        data.actors.front().patrol.value_or(simple_platformer::Patrol{});
    REQUIRE(patrol.secondFeet.x == 56.0F);
    REQUIRE(data.actors[1].definitionName == "bat");
    REQUIRE(data.actors[1].spawnFeet.x == 17.0F);
    REQUIRE(data.pickups.size() == 1);
    REQUIRE(data.pickups.front().stack.item == "key");
    REQUIRE(data.pickups.front().spawnFeet.x == 24.0F);
    REQUIRE(data.exit.spawnFeet.x == 40.0F);
    REQUIRE(data.exit.requirement.has_value());
    REQUIRE_FALSE(data.exit.nextLevel.has_value());
}

TEST_CASE("Level JSON rejects malformed or unknown content", "[app][content][json]")
{
    REQUIRE_THROWS_AS(
        simple_platformer::parseExampleLevelData("not JSON", "broken level"),
        std::invalid_argument);

    REQUIRE_THROWS_AS(
        simple_platformer::parseExampleLevelData(
            R"({
                "map": ["....", "###"],
                "playerSpawnFeet": [8, 8],
                "actors": [],
                "pickups": [],
                "exit": {"spawnFeet": [8, 16]}
            })",
            "ragged level"),
        std::invalid_argument);

    REQUIRE_THROWS_AS(
        simple_platformer::parseExampleLevelData(
            R"({
                "map": ["....", "####"],
                "playerSpawnFeet": [8, 8],
                "actors": [{"definition": "", "spawnFeet": [8, 8]}],
                "pickups": [],
                "exit": {"spawnFeet": [8, 16]}
            })",
            "empty actor name level"),
        std::invalid_argument);

    REQUIRE_THROWS_AS(
        simple_platformer::parseExampleLevelData(
            R"({
                "map": ["....", "####"],
                "playerSpawnCell": [1, 0],
                "actors": [{
                    "definition": "zombie",
                    "spawnCell": [1, 0],
                    "spawnFeet": [24, 16]
                }],
                "pickups": [],
                "exit": {"spawnFeet": [8, 16]}
            })",
            "ambiguous placement"),
        std::invalid_argument);
}

TEST_CASE("Pickup placements choose a definition or an inline stack", "[app][content][json]")
{
    auto root = nlohmann::json::parse(R"({
        "map":["....","####"],"playerSpawnCell":[0,0],"actors":[],
        "pickups":[{"definition":"treasure","spawnCell":[1,0]}],
        "exit":{"spawnCell":[3,0]}
    })");
    const auto parsed = simple_platformer::parseExampleLevelData(root.dump(), "placement.json");
    REQUIRE(parsed.pickups.front().definitionName == "treasure");
    REQUIRE(parsed.pickupReferences.at("pickups[0].definition") == "treasure");
    SECTION("Mixed definition and item")
    {
        root["pickups"][0]["item"] = "key";
    }
    SECTION("Mixed definition and quantity")
    {
        root["pickups"][0]["quantity"] = 2;
    }
    SECTION("Empty definition")
    {
        root["pickups"][0]["definition"] = "";
    }
    REQUIRE_THROWS_AS(
        simple_platformer::parseExampleLevelData(root.dump(), "placement.json"),
        std::invalid_argument);
}

TEST_CASE("Missing level JSON is rejected at the file boundary", "[app][content][json]")
{
    REQUIRE_THROWS_AS(
        simple_platformer::loadExampleLevelData(
            std::filesystem::path("assets/levels/does_not_exist.json")),
        std::invalid_argument);
}
