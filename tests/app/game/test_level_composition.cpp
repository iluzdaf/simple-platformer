#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <stdexcept>
#include <filesystem>

#include "game/level_composition.hpp"
#include "content/game_catalogs.hpp"
#include "content/level_catalog.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/world/level_exit.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/actor_components.hpp"

TEST_CASE("A catalog entry must reference an existing level file", "[app][content]")
{
    const auto levelCatalog = simple_platformer::parseLevelCatalog(
        R"({
            "startLevel": 1,
            "levels": [{"number": 1, "file": "missing.json"}]
        })",
        "test catalog",
        "tests/fixtures/levels");
    const auto gameCatalogs = simple_platformer::loadGameCatalogs("tests/fixtures/catalogs");

    REQUIRE_THROWS_AS(
        simple_platformer::composeGameLevel(levelCatalog, 1, 0, gameCatalogs),
        std::invalid_argument);
}

TEST_CASE("A level's cells become the feet of those cells on its map", "[app][content]")
{
    const auto levelCatalog = simple_platformer::loadLevelCatalog(
        std::filesystem::path("tests/fixtures/levels/levels.json"));
    const auto gameCatalogs = simple_platformer::loadGameCatalogs("tests/fixtures/catalogs");
    const auto gameLevel = simple_platformer::composeGameLevel(levelCatalog, 10, 0, gameCatalogs);
    const int tileSize = gameLevel.map.tileSize();

    REQUIRE(gameLevel.playerSpawnFeet == simple_platformer::feetInCell(tileSize, {1, 2}));
    REQUIRE(gameLevel.world.pickups().size() == 1);
    REQUIRE(
        simple_platformer::feetOf(gameLevel.world.pickups().front().body.bounds) ==
        simple_platformer::feetInCell(tileSize, {2, 2}));
    const auto& levelExit = gameLevel.world.exit();
    if (!levelExit.has_value())
    {
        throw std::logic_error("The opening level must have an exit");
    }
    REQUIRE(
        simple_platformer::feetOf(levelExit->bounds) ==
        simple_platformer::feetInCell(tileSize, {5, 2}));
}

TEST_CASE("A level composes an actor from its catalog definition", "[app][actors]")
{
    const auto levelCatalog = simple_platformer::parseLevelCatalog(
        R"({"startLevel":1,"levels":[{"number":1,"file":"actor_placement.json"}]})",
        "fixture",
        "tests/fixtures/levels");
    const auto gameCatalogs = simple_platformer::loadGameCatalogs("tests/fixtures/catalogs");
    auto gameLevel = simple_platformer::composeGameLevel(levelCatalog, 1, 0, gameCatalogs);
    REQUIRE(gameLevel.world.actors().size() == 1);
    auto& actor = gameLevel.world.actors().front();
    REQUIRE(tests::platformerMovement(actor).config.maximumSpeed == 23);
    REQUIRE(simple_platformer::feetOf(actor.body.bounds).x == 56);
}

TEST_CASE("Level composition reports unknown actor definitions", "[app][actors]")
{
    const auto invalidLevelCatalog = simple_platformer::parseLevelCatalog(
        R"({"startLevel":1,"levels":[{"number":1,"file":"unknown_actor.json"}]})",
        "fixture",
        "tests/fixtures/levels");
    const auto gameCatalogs = simple_platformer::loadGameCatalogs("tests/fixtures/catalogs");
    REQUIRE_THROWS_WITH(
        simple_platformer::composeGameLevel(invalidLevelCatalog, 1, 0, gameCatalogs),
        Catch::Matchers::ContainsSubstring(
            "objectLegend.Z.definition: unknown actor definition 'missing'"));
}
