#include <catch2/catch_test_macros.hpp>

#include <glm/vec2.hpp>

#include "content/level_data.hpp"
#include "simple_platformer/math/coordinates.hpp"

TEST_CASE("Actor markers append to explicit placements over empty terrain", "[app][content][json]")
{
    const auto data = simple_platformer::parseLevelData(
        R"({
            "tileLegend": {".": "empty", "#": "stone", "G": "grass"},
            "objectLegend": {
                "P": {"type": "player"},
                "Z": {"type": "actor", "definition": "zombie",
                      "patrol": {"firstCell": [1, 0], "secondCell": [2, 0]}},
                "B": {"type": "actor", "definition": "bat"},
                "S": {"type": "actor", "definition": "zombie_soldier"},
                "E": {"type": "exit", "definition": "test_door"}
            },
            "map": ["PZZBSEG.", "########"],
            "actors": [{"definition": "zombie", "spawnCell": [7, 0]}]
        })",
        "markers");

    REQUIRE(
        data.playerSpawn ==
        simple_platformer::LevelPosition{simple_platformer::GridPosition{0, 0}});
    REQUIRE(data.actors.size() == 5);
    REQUIRE(
        data.actors[0].spawn ==
        simple_platformer::LevelPosition{simple_platformer::GridPosition{7, 0}});
    REQUIRE(
        data.actors[1].spawn ==
        simple_platformer::LevelPosition{simple_platformer::GridPosition{1, 0}});
    REQUIRE(
        data.actors[2].spawn ==
        simple_platformer::LevelPosition{simple_platformer::GridPosition{2, 0}});
    REQUIRE(data.actors[1].patrol.has_value());
    REQUIRE(data.actors[3].definitionName == "bat");
    REQUIRE(data.actors[4].definitionName == "zombie_soldier");
    REQUIRE(data.tileLegend.at('Z') == "empty");
    REQUIRE(data.tileLegend.at('G') == "grass");
}

TEST_CASE("Pickup and exit markers retain authored values", "[app][content][json]")
{
    const auto data = simple_platformer::parseLevelData(
        R"({
            "tileLegend": {".": "empty", "#": "stone"},
            "objectLegend": {
                "P": {"type": "player"},
                "K": {"type": "pickup", "item": "key", "quantity": 2, "bodySize": [8, 8]},
                "E": {"type": "exit", "definition": "test_door",
                      "requirement": {"item": "key", "quantity": 1},
                      "consumeItem": true, "nextLevel": 2}
            },
            "map": ["PKKE", "####"],
            "pickups": [{"item": "coin", "quantity": 3, "bodySize": [8, 8],
                         "spawnFeet": [136, 8]}]
        })",
        "markers");

    REQUIRE(data.pickups.size() == 3);
    REQUIRE(data.pickups[0].spawn == simple_platformer::LevelPosition{glm::vec2{136.0F, 8.0F}});
    REQUIRE(data.pickups[1].stack.item == "key");
    REQUIRE(data.pickups[1].stack.quantity == 2);
    REQUIRE(
        data.pickups[2].spawn ==
        simple_platformer::LevelPosition{simple_platformer::GridPosition{2, 0}});
    REQUIRE(
        data.exit.spawn == simple_platformer::LevelPosition{simple_platformer::GridPosition{3, 0}});
    REQUIRE(data.exit.requirement.has_value());
    REQUIRE(data.exit.consumeItem);
    REQUIRE(data.exit.nextLevel == 2);
}

TEST_CASE("Object-only levels may omit explicit placement arrays", "[app][content][json]")
{
    const auto data = simple_platformer::parseLevelData(
        R"({
            "tileLegend": {".": "empty", "#": "stone"},
            "objectLegend": {
                "P": {"type": "player"},
                "E": {"type": "exit", "definition": "test_door"}
            },
            "map": ["PE"]
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
            "tileLegend": {".": "empty", "G": "grass", "X": "glass"},
            "map": [".GX"],
            "playerSpawnCell": [0, 0],
            "actors": [],
            "pickups": [],
            "exit": {"definition": "test_door", "spawnCell": [2, 0]}
        })",
        "custom level");

    REQUIRE(data.tileLegend.at('G') == "grass");
    REQUIRE(data.mapRows.front() == ".GX");
}

TEST_CASE(
    "Actor placements retain definition references and patrol coordinates",
    "[app][content][json]")
{
    const auto data = simple_platformer::parseLevelData(
        R"({
            "tileLegend": {".": "empty", "#": "stone"},
            "map": ["....", "####"],
            "playerSpawnCell": [1, 0],
            "actors": [
                {"definition": "zombie", "spawnCell": [2, 0],
                 "patrol": {"firstCell": [2, 0], "secondCell": [3, 0]}},
                {"definition": "bat", "spawnFeet": [17, 9],
                 "patrol": {"firstFeet": [17, 9], "secondFeet": [25, 13]}}
            ],
            "exit": {"definition": "test_door", "spawnCell": [3, 0]}
        })",
        "test level");

    REQUIRE(data.mapRows.size() == 2);
    REQUIRE(
        data.playerSpawn ==
        simple_platformer::LevelPosition{simple_platformer::GridPosition{1, 0}});
    REQUIRE(data.actors.size() == 2);
    REQUIRE(data.actors[0].definitionName == "zombie");
    REQUIRE(
        data.actors[0].spawn ==
        simple_platformer::LevelPosition{simple_platformer::GridPosition{2, 0}});
    REQUIRE(data.actors[0].patrol.has_value());
    REQUIRE(
        data.actors[0].patrol.value_or(simple_platformer::PatrolPlacement{}).second ==
        simple_platformer::LevelPosition{simple_platformer::GridPosition{3, 0}});
    REQUIRE(data.actors[1].definitionName == "bat");
    REQUIRE(data.actors[1].spawn == simple_platformer::LevelPosition{glm::vec2{17.0F, 9.0F}});
}

TEST_CASE("Explicit pickups and exits retain item requirements", "[app][content][json]")
{
    const auto data = simple_platformer::parseLevelData(
        R"({
            "tileLegend": {".": "empty", "#": "stone"},
            "map": ["....", "####"],
            "playerSpawnCell": [1, 0],
            "pickups": [{"item": "key", "quantity": 1, "bodySize": [8, 8],
                         "spawnCell": [1, 0]}],
            "exit": {"definition": "test_door", "spawnCell": [2, 0],
                     "requirement": {"item": "key", "quantity": 1}}
        })",
        "test level");

    REQUIRE(data.pickups.size() == 1);
    REQUIRE(data.pickups[0].stack.item == "key");
    REQUIRE(
        data.pickups[0].spawn ==
        simple_platformer::LevelPosition{simple_platformer::GridPosition{1, 0}});
    REQUIRE(
        data.exit.spawn == simple_platformer::LevelPosition{simple_platformer::GridPosition{2, 0}});
    REQUIRE(data.exit.requirement.has_value());
    REQUIRE_FALSE(data.exit.nextLevel.has_value());
}

TEST_CASE("Pickup placements can reference a definition", "[app][content][json]")
{
    const auto data = simple_platformer::parseLevelData(
        R"({
            "tileLegend": {".": "empty", "#": "stone"},
            "map": ["....", "####"],
            "playerSpawnCell": [0, 0],
            "actors": [],
            "pickups": [{"definition": "treasure", "spawnCell": [1, 0]}],
            "exit": {"definition": "test_door", "spawnCell": [3, 0]}
        })",
        "placement.json");

    REQUIRE(data.pickups[0].definitionName == "treasure");
    REQUIRE(data.pickupReferences.at("pickups[0].definition") == "treasure");
}

TEST_CASE("Inline pickup placements retain their body size", "[app][content][json]")
{
    const auto data = simple_platformer::parseLevelData(
        R"({
            "tileLegend": {".": "empty", "#": "stone"},
            "map": ["....", "####"],
            "playerSpawnCell": [0, 0],
            "actors": [],
            "pickups": [{"item": "key", "quantity": 1, "bodySize": [10, 12],
                         "spawnCell": [1, 0]}],
            "exit": {"definition": "test_door", "spawnCell": [3, 0]}
        })",
        "placement.json");

    REQUIRE(data.pickups[0].bodySize == glm::vec2{10.0F, 12.0F});
}
