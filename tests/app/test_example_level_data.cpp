#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <stdexcept>

#include "game/example_items.hpp"
#include "game/example_level_data.hpp"
#include "simple_platformer/npc/npc.hpp"

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
                    "type": "zombie",
                    "spawnCell": [2, 0],
                    "patrol": {
                        "firstCell": [2, 0],
                        "secondCell": [3, 0]
                    }
                },
                {
                    "type": "bat",
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
    REQUIRE(data.actors.front().type == simple_platformer::ExampleActorType::Zombie);
    REQUIRE(data.actors.front().spawnFeet.x == 40.0F);
    REQUIRE(data.actors.front().patrol.has_value());
    const simple_platformer::Patrol patrol =
        data.actors.front().patrol.value_or(simple_platformer::Patrol{});
    REQUIRE(patrol.secondFeet.x == 56.0F);
    REQUIRE(data.actors[1].type == simple_platformer::ExampleActorType::Bat);
    REQUIRE(data.actors[1].spawnFeet.x == 17.0F);
    REQUIRE(data.pickups.size() == 1);
    REQUIRE(data.pickups.front().stack.item == simple_platformer::Key);
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
                "actors": [{"type": "ghost", "spawnFeet": [8, 8]}],
                "pickups": [],
                "exit": {"spawnFeet": [8, 16]}
            })",
            "unknown actor level"),
        std::invalid_argument);

    REQUIRE_THROWS_AS(
        simple_platformer::parseExampleLevelData(
            R"({
                "map": ["....", "####"],
                "playerSpawnCell": [1, 0],
                "actors": [{
                    "type": "zombie",
                    "spawnCell": [1, 0],
                    "spawnFeet": [24, 16]
                }],
                "pickups": [],
                "exit": {"spawnFeet": [8, 16]}
            })",
            "ambiguous placement"),
        std::invalid_argument);
}

TEST_CASE("Missing level JSON is rejected at the file boundary", "[app][content][json]")
{
    REQUIRE_THROWS_AS(
        simple_platformer::loadExampleLevelData(
            std::filesystem::path("assets/levels/does_not_exist.json")),
        std::invalid_argument);
}
