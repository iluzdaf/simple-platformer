#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <stdexcept>
#include <glm/vec2.hpp>
#include <nlohmann/json.hpp>

#include "content/level_data.hpp"
#include "simple_platformer/math/coordinates.hpp"

TEST_CASE("Level placements reject overflowing integers and unknown fields", "[app][content][json]")
{
    auto root = nlohmann::json::parse(R"({
        "tileLegend": {".": "empty", "#": "stone"},
        "map":["....","####"],"playerSpawnCell":[0,0],
        "actors":[{"definition":"guard","spawnCell":[1,0]}],
        "pickups":[{"item":"key","quantity":1,"spawnCell":[2,0]}],
        "exit":{"definition":"door","spawnCell":[3,0]}
    })");
    SECTION("Oversized cell")
    {
        root["playerSpawnCell"][0] = 4294967296LL;
    }
    SECTION("Undersized cell")
    {
        root["playerSpawnCell"][1] = -4294967296LL;
    }
    SECTION("Overflowing quantity")
    {
        root["pickups"][0]["quantity"] = 4294967297LL;
    }
    SECTION("Overflowing destination")
    {
        root["exit"]["nextLevel"] = 4294967297LL;
    }
    SECTION("Root typo")
    {
        root["actorrs"] = nlohmann::json::array();
    }
    SECTION("Actor typo")
    {
        root["actors"][0]["patroll"] = {};
    }
    SECTION("Patrol typo")
    {
        root["actors"][0]["patrol"] = {
            {"firstCell", {0, 0}}, {"secondCell", {1, 0}}, {"speeed", 1}};
    }
    SECTION("Pickup typo")
    {
        root["pickups"][0]["quantitty"] = 3;
    }
    SECTION("Exit typo")
    {
        root["exit"]["nextLevell"] = 2;
    }
    SECTION("Requirement typo")
    {
        root["exit"]["requirement"] = {{"item", "key"}, {"quantity", 1}, {"consumme", true}};
    }
    SECTION("Unused actor legend typo")
    {
        root["objectLegend"]["Z"] = {{"type", "actor"}, {"definition", "guard"}, {"patroll", true}};
    }
    SECTION("Unused player legend typo")
    {
        root["objectLegend"]["P"] = {{"type", "player"}, {"health", 4}};
    }
    SECTION("Unused pickup legend typo")
    {
        root["objectLegend"]["K"] = {{"type", "pickup"}, {"definition", "key"}, {"quantitty", 3}};
    }
    SECTION("Unused exit legend typo")
    {
        root["objectLegend"]["E"] = {{"type", "exit"}, {"definition", "door"}, {"nextLevell", 2}};
    }
    REQUIRE_THROWS_WITH(
        simple_platformer::parseLevelData(root.dump(), "placements.json"),
        Catch::Matchers::ContainsSubstring("placements.json:"));
}

TEST_CASE("Level diagnostics identify authored fields and map cells", "[app][content][json]")
{
    auto level = nlohmann::json::parse(R"({
        "tileLegend": {".": "empty", "#": "stone"},
        "objectLegend":{"P":{"type":"player"},"E":{"type": "exit", "definition": "test_door"}},
        "map":["PE"]
    })");
    SECTION("Unknown map symbol includes row column and symbol")
    {
        level["map"] = {"P?E"};
        REQUIRE_THROWS_WITH(
            simple_platformer::parseLevelData(level.dump(), "level.json"),
            "level.json: map[0][1]: unknown symbol '?'; define it in tileLegend or objectLegend");
    }
    SECTION("Repeated marker identifies both placements")
    {
        level["map"] = {"PPE"};
        REQUIRE_THROWS_WITH(
            simple_platformer::parseLevelData(level.dump(), "level.json"),
            "level.json: map[0][1]: second player marker 'P'; player already placed at map[0][0]");
    }
    SECTION("Exit conflict identifies explicit placement")
    {
        level["exit"] = {{"definition", "test_door"}, {"spawnCell", {1, 0}}};
        REQUIRE_THROWS_WITH(
            simple_platformer::parseLevelData(level.dump(), "level.json"),
            "level.json: map[0][1]: second exit marker 'E'; exit already placed at exit");
    }
    SECTION("Legend errors do not report generated array indices")
    {
        level["objectLegend"]["K"] = {{"type", "pickup"}, {"item", "key"}, {"quantity", 0}};
        REQUIRE_THROWS_WITH(
            simple_platformer::parseLevelData(level.dump(), "level.json"),
            "level.json: objectLegend.K.quantity: expected a positive integer, got 0");
    }
    SECTION("Exit settings identify the legend field")
    {
        level["objectLegend"]["E"]["consumeItem"] = 1;
        REQUIRE_THROWS_WITH(
            simple_platformer::parseLevelData(level.dump(), "level.json"),
            "level.json: objectLegend.E.consumeItem: expected true or false");
    }
    SECTION("Missing exit definition identifies the legend entry")
    {
        level["objectLegend"]["E"].erase("definition");
        REQUIRE_THROWS_WITH(
            simple_platformer::parseLevelData(level.dump(), "level.json"),
            "level.json: objectLegend.E: missing 'definition'");
    }
    SECTION("Empty exit definition identifies its field")
    {
        level["objectLegend"]["E"]["definition"] = "";
        REQUIRE_THROWS_WITH(
            simple_platformer::parseLevelData(level.dump(), "level.json"),
            "level.json: objectLegend.E.definition: exit definition name cannot be empty");
    }
    SECTION("Row width includes expected and actual widths")
    {
        level["map"] = {"PE", "."};
        REQUIRE_THROWS_WITH(
            simple_platformer::parseLevelData(level.dump(), "level.json"),
            "level.json: map[1]: expected 2 columns, got 1");
    }
    SECTION("Explicit entries retain their array paths")
    {
        level["actors"] = nlohmann::json::array({{{"definition", ""}, {"spawnCell", {0, 0}}}});
        REQUIRE_THROWS_WITH(
            simple_platformer::parseLevelData(level.dump(), "level.json"),
            "level.json: actors[0].definition: actor definition name cannot be empty");
    }
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

TEST_CASE(
    "Object markers expand into ordinary placements over empty terrain",
    "[app][content][json]")
{
    const auto data = simple_platformer::parseLevelData(
        R"({
        "tileLegend":{".":"empty", "#":"stone", "G":"grass"},
        "objectLegend":{
            "P":{"type":"player"},
            "Z":{"type":"actor", "definition":"zombie", "patrol":{"firstCell":[1,0],"secondCell":[2,0]}},
            "B":{"type":"actor", "definition":"bat"}, "S":{"type":"actor", "definition":"zombie_soldier"},
            "K":{"type":"pickup","item":"key","quantity":2},
            "E":{"type": "exit", "definition": "test_door","requirement":{"item":"key","quantity":1},
                 "consumeItem":true,"nextLevel":2}
        },
        "map":["PZZBSKKEG", "#########"],
        "actors":[{"definition":"zombie","spawnCell":[8,0]}],
        "pickups":[{"item":"coin","quantity":3,"spawnFeet":[136,8]}]
    })",
        "markers");
    REQUIRE(
        data.playerSpawn ==
        simple_platformer::LevelPosition{simple_platformer::GridPosition{0, 0}});
    REQUIRE(data.actors.size() == 5);
    REQUIRE(
        data.actors[0].spawn ==
        simple_platformer::LevelPosition{simple_platformer::GridPosition{8, 0}});
    REQUIRE(
        data.actors[1].spawn ==
        simple_platformer::LevelPosition{simple_platformer::GridPosition{1, 0}});
    REQUIRE(
        data.actors[2].spawn ==
        simple_platformer::LevelPosition{simple_platformer::GridPosition{2, 0}});
    REQUIRE(data.actors[1].patrol.has_value());
    REQUIRE(data.actors[3].definitionName == "bat");
    REQUIRE(data.actors[4].definitionName == "zombie_soldier");
    REQUIRE(data.pickups.size() == 3);
    REQUIRE(data.pickups[1].stack.item == "key");
    REQUIRE(data.pickups[1].stack.quantity == 2);
    REQUIRE(data.pickups[0].spawn == simple_platformer::LevelPosition{glm::vec2{136.0F, 8.0F}});
    REQUIRE(
        data.pickups[2].spawn ==
        simple_platformer::LevelPosition{simple_platformer::GridPosition{6, 0}});
    REQUIRE(
        data.exit.spawn == simple_platformer::LevelPosition{simple_platformer::GridPosition{7, 0}});
    REQUIRE(data.exit.requirement.has_value());
    REQUIRE(data.exit.consumeItem);
    REQUIRE(data.exit.nextLevel == 2);
    REQUIRE(data.tileLegend.at('Z') == "empty");
    REQUIRE(data.tileLegend.at('G') == "grass");
}

TEST_CASE("Object legends reject ambiguous or invalid placements", "[app][content][json]")
{
    auto level = nlohmann::json::parse(R"({
        "tileLegend": {".": "empty", "#": "stone"},
        "objectLegend":{"P":{"type":"player"},"E":{"type": "exit", "definition": "test_door"}},
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
        level["exit"] = {{"definition", "test_door"}, {"spawnCell", {1, 0}}};
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
        simple_platformer::parseLevelData(level.dump(), "bad markers"), std::invalid_argument);
}

TEST_CASE("Object-only levels may omit explicit placement arrays", "[app][content][json]")
{
    const auto data = simple_platformer::parseLevelData(
        R"({
        "tileLegend": {".": "empty", "#": "stone"},
        "objectLegend":{"P":{"type":"player"},"E":{"type": "exit", "definition": "test_door"}},
        "map":["PE"]
    })",
        "markers");
    REQUIRE(data.actors.empty());
    REQUIRE(data.pickups.empty());
    REQUIRE(
        data.exit.spawn == simple_platformer::LevelPosition{simple_platformer::GridPosition{1, 0}});
}

TEST_CASE("Level JSON accepts a custom tile legend", "[app][content][json]")
{
    const auto data = simple_platformer::parseLevelData(
        R"({
        "tileLegend": {".":"empty", "G":"grass", "X":"glass"},
        "map":[".GX"], "playerSpawnCell":[0,0], "actors":[], "pickups":[],
        "exit": {"definition": "test_door","spawnCell":[2,0]}
    })",
        "custom level");
    REQUIRE(data.tileLegend.at('G') == "grass");
    REQUIRE(data.mapRows.front() == ".GX");
    REQUIRE_THROWS_AS(
        simple_platformer::parseLevelData(
            R"({
        "tileLegend":{"long":"grass"}, "map":["."]
    })",
            "bad legend"),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::parseLevelData(
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
    const auto data = simple_platformer::parseLevelData(
        R"({
            "tileLegend": {".": "empty", "#": "stone"},
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
            "exit": {"definition": "test_door",
                "spawnCell": [2, 0],
                "requirement": {"item": "key", "quantity": 1}
            }
        })",
        "test level");

    REQUIRE(data.mapRows.size() == 2);
    REQUIRE(
        data.playerSpawn ==
        simple_platformer::LevelPosition{simple_platformer::GridPosition{1, 0}});
    REQUIRE(data.actors.size() == 2);
    REQUIRE(data.actors.front().definitionName == "zombie");
    REQUIRE(
        data.actors.front().spawn ==
        simple_platformer::LevelPosition{simple_platformer::GridPosition{2, 0}});
    REQUIRE(data.actors.front().patrol.has_value());
    REQUIRE(
        data.actors.front().patrol.value_or(simple_platformer::PatrolPlacement{}).second ==
        simple_platformer::LevelPosition{simple_platformer::GridPosition{3, 0}});
    REQUIRE(data.actors[1].definitionName == "bat");
    REQUIRE(data.actors[1].spawn == simple_platformer::LevelPosition{glm::vec2{17.0F, 9.0F}});
    REQUIRE(data.pickups.size() == 1);
    REQUIRE(data.pickups.front().stack.item == "key");
    REQUIRE(
        data.pickups.front().spawn ==
        simple_platformer::LevelPosition{simple_platformer::GridPosition{1, 0}});
    REQUIRE(
        data.exit.spawn == simple_platformer::LevelPosition{simple_platformer::GridPosition{2, 0}});
    REQUIRE(data.exit.requirement.has_value());
    REQUIRE_FALSE(data.exit.nextLevel.has_value());
}

TEST_CASE("Level JSON rejects malformed or unknown content", "[app][content][json]")
{
    REQUIRE_THROWS_AS(
        simple_platformer::parseLevelData("not JSON", "broken level"), std::invalid_argument);

    REQUIRE_THROWS_AS(
        simple_platformer::parseLevelData(
            R"({
                "tileLegend": {".": "empty", "#": "stone"},
                "map": ["....", "###"],
                "playerSpawnFeet": [8, 8],
                "actors": [],
                "pickups": [],
                "exit": {"definition": "test_door","spawnFeet": [8, 16]}
            })",
            "ragged level"),
        std::invalid_argument);

    REQUIRE_THROWS_AS(
        simple_platformer::parseLevelData(
            R"({
                "tileLegend": {".": "empty", "#": "stone"},
                "map": ["....", "####"],
                "playerSpawnFeet": [8, 8],
                "actors": [{"definition": "", "spawnFeet": [8, 8]}],
                "pickups": [],
                "exit": {"definition": "test_door","spawnFeet": [8, 16]}
            })",
            "empty actor name level"),
        std::invalid_argument);

    REQUIRE_THROWS_AS(
        simple_platformer::parseLevelData(
            R"({
                "tileLegend": {".": "empty", "#": "stone"},
                "map": ["....", "####"],
                "playerSpawnCell": [1, 0],
                "actors": [{
                    "definition": "zombie",
                    "spawnCell": [1, 0],
                    "spawnFeet": [24, 16]
                }],
                "pickups": [],
                "exit": {"definition": "test_door","spawnFeet": [8, 16]}
            })",
            "ambiguous placement"),
        std::invalid_argument);
}

TEST_CASE("Pickup placements choose a definition or an inline stack", "[app][content][json]")
{
    auto root = nlohmann::json::parse(R"({
        "tileLegend": {".": "empty", "#": "stone"},
        "map":["....","####"],"playerSpawnCell":[0,0],"actors":[],
        "pickups":[{"definition":"treasure","spawnCell":[1,0]}],
        "exit": {"definition": "test_door","spawnCell":[3,0]}
    })");
    const auto parsed = simple_platformer::parseLevelData(root.dump(), "placement.json");
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
        simple_platformer::parseLevelData(root.dump(), "placement.json"), std::invalid_argument);
}

TEST_CASE("Missing level JSON is rejected at the file boundary", "[app][content][json]")
{
    REQUIRE_THROWS_AS(
        simple_platformer::loadLevelData(std::filesystem::path("assets/does_not_exist.json")),
        std::invalid_argument);
}

TEST_CASE("Level JSON requires a tile legend", "[app][content][json]")
{
    REQUIRE_THROWS_WITH(
        simple_platformer::parseLevelData(
            R"({
                "map": ["....", "####"],
                "playerSpawnCell": [1, 0],
                "actors": [],
                "pickups": [],
                "exit": {"definition": "test_door", "spawnCell": [3, 0]}
            })",
            "no legend"),
        Catch::Matchers::ContainsSubstring("tileLegend"));
}
