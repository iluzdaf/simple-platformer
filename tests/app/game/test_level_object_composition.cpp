#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <stdexcept>

#include "content/game_catalogs.hpp"
#include "content/item_catalog.hpp"
#include "content/level_catalog.hpp"
#include "game/level_composition.hpp"
#include "simple_platformer/math/aabb.hpp"

TEST_CASE("Named pickups and exit requirements resolve through level composition", "[app][pickups]")
{
    const auto levelCatalog = simple_platformer::parseLevelCatalog(
        R"({"startLevel":1,"levels":[{"number":1,"file":"pickup_placement.json"}]})",
        "fixture",
        "tests/fixtures/levels");
    const auto gameCatalogs = simple_platformer::loadGameCatalogs("tests/fixtures/catalogs");
    const auto gameLevel = simple_platformer::composeGameLevel(levelCatalog, 1, 7, gameCatalogs);
    const auto& itemCatalog = gameCatalogs.items;
    REQUIRE(gameLevel.world.pickups().size() == 2);
    REQUIRE(
        gameLevel.world.pickups()[0].stack.item ==
        simple_platformer::itemDefinition(itemCatalog, "medicine").id);
    REQUIRE(
        gameLevel.world.pickups()[1].stack.item ==
        simple_platformer::itemDefinition(itemCatalog, "key").id);
    REQUIRE(gameLevel.world.pickups()[1].body.bounds.size == glm::vec2{10, 12});
    const auto& exit = gameLevel.world.exit();
    if (!exit || !exit->requirement)
    {
        throw std::logic_error("Missing fixture exit requirement");
    }
    REQUIRE(exit->requirement->item == simple_platformer::itemDefinition(itemCatalog, "key").id);
}

TEST_CASE("Exit item references resolve through the item catalog", "[app][pickups]")
{
    const auto levelCatalog = simple_platformer::parseLevelCatalog(
        R"({"startLevel":1,"levels":[{"number":1,"file":"unknown_item.json"}]})",
        "fixture",
        "tests/fixtures/levels");
    const auto gameCatalogs = simple_platformer::loadGameCatalogs("tests/fixtures/catalogs");
    REQUIRE_THROWS_WITH(
        simple_platformer::composeGameLevel(levelCatalog, 1, 0, gameCatalogs),
        Catch::Matchers::ContainsSubstring("unknown_item.json:"));
    REQUIRE_THROWS_WITH(
        simple_platformer::composeGameLevel(levelCatalog, 1, 0, gameCatalogs),
        Catch::Matchers::ContainsSubstring("requirement.item: unknown item 'missing'"));
}

TEST_CASE("Unknown unused pickup legend references identify their source", "[app][pickups]")
{
    const auto levelCatalog = simple_platformer::parseLevelCatalog(
        R"({"startLevel":1,"levels":[{"number":1,"file":"unknown_pickup.json"}]})",
        "fixture",
        "tests/fixtures/levels");
    const auto gameCatalogs = simple_platformer::loadGameCatalogs("tests/fixtures/catalogs");
    REQUIRE_THROWS_WITH(
        simple_platformer::composeGameLevel(levelCatalog, 1, 0, gameCatalogs),
        Catch::Matchers::ContainsSubstring(
            "unknown_pickup.json: objectLegend.K.definition: unknown pickup definition 'missing'"));
}

TEST_CASE("Level composition reuses the supplied session item catalog", "[app][pickups]")
{
    const auto levelCatalog =
        simple_platformer::loadLevelCatalog("tests/fixtures/levels/levels.json");
    // An extra item shifts generated IDs; both levels must use this session's item catalog.
    const auto itemCatalog = simple_platformer::parseItemCatalog(
        R"({"items":{
        "aaa":{"name":"Extra item","icon":{"position":[0,0],"size":[8,8]},"maximumStack":1},
        "key":{"name":"Session key","icon":{"position":[8,0],"size":[8,8]},"maximumStack":2},
        "medicine":{"name":"Medicine","icon":{"position":[16,0],"size":[8,8]},"maximumStack":3}
    }})",
        "session items");
    auto gameCatalogs = simple_platformer::loadGameCatalogs("tests/fixtures/catalogs");
    gameCatalogs.items = itemCatalog;
    const auto keyId = simple_platformer::itemDefinition(itemCatalog, "key").id;
    const auto firstLevel = simple_platformer::composeGameLevel(levelCatalog, 10, 0, gameCatalogs);
    const auto secondLevel = simple_platformer::composeGameLevel(levelCatalog, 25, 0, gameCatalogs);
    REQUIRE(firstLevel.world.pickups().front().stack.item == keyId);
    REQUIRE(firstLevel.world.itemDefinition(keyId).name == "Session key");
    REQUIRE(secondLevel.world.itemDefinition(keyId).name == "Session key");
}

TEST_CASE("Level exit placement combines a definition with completion settings", "[app][exits]")
{
    const auto levelCatalog =
        simple_platformer::loadLevelCatalog("tests/fixtures/levels/levels.json");
    const auto gameCatalogs = simple_platformer::loadGameCatalogs("tests/fixtures/catalogs");
    const auto gameLevel = simple_platformer::composeGameLevel(levelCatalog, 10, 7, gameCatalogs);
    const auto& exit = gameLevel.world.exit();
    if (!exit || !exit->sprite)
    {
        throw std::logic_error("Missing fixture exit");
    }
    REQUIRE(exit->bounds.size == glm::vec2{12, 24});
    REQUIRE(exit->sprite->textureId == 7);
    REQUIRE(exit->nextLevel == 25);
    REQUIRE(exit->requirement.has_value());
}

TEST_CASE("Unknown unused exit definitions retain the legend path", "[app][exits]")
{
    const auto levelCatalog = simple_platformer::parseLevelCatalog(
        R"({"startLevel":1,"levels":[{"number":1,"file":"unknown_exit.json"}]})",
        "fixture",
        "tests/fixtures/levels");
    const auto gameCatalogs = simple_platformer::loadGameCatalogs("tests/fixtures/catalogs");
    REQUIRE_THROWS_WITH(
        simple_platformer::composeGameLevel(levelCatalog, 1, 0, gameCatalogs),
        Catch::Matchers::ContainsSubstring(
            "unknown_exit.json: objectLegend.E.definition: unknown exit definition 'missing'"));
}
