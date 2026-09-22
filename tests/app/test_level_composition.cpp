#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <filesystem>

#include "game/level_composition.hpp"
#include "content/game_catalogs.hpp"
#include "content/level_catalog.hpp"
#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/world/level_exit.hpp"
#include "simple_platformer/world/level_validation.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/add_player.hpp"

TEST_CASE("Every catalog level can be composed", "[app][content]")
{
    const auto catalog = simple_platformer::loadLevelCatalog("assets/levels.json");

    REQUIRE_FALSE(catalog.levels.empty());
    for (const simple_platformer::LevelCatalogEntry& entry : catalog.levels)
    {
        const auto content = simple_platformer::composeGameLevel(catalog, entry.number, 0);
        REQUIRE(content.number == entry.number);

        const auto& levelExit = content.world.exit();
        if (!levelExit.has_value())
        {
            throw std::logic_error("A composed level must have an exit");
        }
        const simple_platformer::LevelExit& exit = levelExit.value();
        if (exit.nextLevel.has_value())
        {
            REQUIRE_NOTHROW(simple_platformer::levelPath(catalog, exit.nextLevel.value()));
        }
    }
}

TEST_CASE("Every catalog level has valid actor placement", "[app][content]")
{
    const auto catalog = simple_platformer::loadLevelCatalog("assets/levels.json");
    const auto catalogs = simple_platformer::loadGameCatalogs(catalog.levelDirectory);
    for (const simple_platformer::LevelCatalogEntry& entry : catalog.levels)
    {
        auto content = simple_platformer::composeGameLevel(catalog, entry.number, 0, catalogs);
        simple_platformer::Actor player = simple_platformer::composePlayer(catalogs, 0);
        simple_platformer::placeFeetAt(player.body.bounds, content.playerSpawnFeet);
        tests::addPlayer(content.world, player);

        REQUIRE_NOTHROW(
            simple_platformer::validateLevelActors(content.map, content.world, content.number));
    }
}

TEST_CASE("A catalog entry must reference an existing level file", "[app][content]")
{
    const auto catalog = simple_platformer::parseLevelCatalog(
        R"({
            "startLevel": 1,
            "levels": [{"number": 1, "file": "missing.json"}]
        })",
        "test catalog",
        "tests/fixtures");

    REQUIRE_THROWS_AS(simple_platformer::composeGameLevel(catalog, 1, 0), std::invalid_argument);
}

TEST_CASE("Session composition does not reload shared catalogue files", "[app][content]")
{
    auto levels = simple_platformer::loadLevelCatalog("tests/fixtures/levels.json");
    const auto catalogs = simple_platformer::loadGameCatalogs(levels.levelDirectory);
    // Keep level files reachable, but give shared catalogue reads nowhere to succeed.
    // Absolute paths here deliberately bypass the JSON loader's relative-path requirement.
    for (auto& entry : levels.levels)
    {
        entry.relativeFile = std::filesystem::absolute(levels.levelDirectory / entry.relativeFile);
    }
    levels.levelDirectory = "tests/fixtures/no-shared-catalogues";
    REQUIRE_NOTHROW(simple_platformer::composePlayer(catalogs, 0));
    for (const auto& entry : levels.levels)
    {
        REQUIRE_NOTHROW(simple_platformer::composeGameLevel(levels, entry.number, 0, catalogs));
    }
}

TEST_CASE("A level's cells become the feet of those cells on its map", "[app][content]")
{
    const auto catalog =
        simple_platformer::loadLevelCatalog(std::filesystem::path("tests/fixtures/levels.json"));
    const auto content = simple_platformer::composeGameLevel(catalog, 10, 0);
    const int tileSize = content.map.tileSize();

    REQUIRE(content.playerSpawnFeet == simple_platformer::feetInCell(tileSize, {1, 2}));
    REQUIRE(content.world.pickups().size() == 1);
    REQUIRE(
        simple_platformer::feetOf(content.world.pickups().front().body.bounds) ==
        simple_platformer::feetInCell(tileSize, {2, 2}));
    const auto& levelExit = content.world.exit();
    if (!levelExit.has_value())
    {
        throw std::logic_error("The opening level must have an exit");
    }
    REQUIRE(
        simple_platformer::feetOf(levelExit->bounds) ==
        simple_platformer::feetInCell(tileSize, {5, 2}));
}
