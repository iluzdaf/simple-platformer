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
    const auto catalog = simple_platformer::parseLevelCatalog(
        R"({"startLevel":1,"levels":[{"number":1,"file":"pickup_placement.json"}]})",
        "fixture",
        "tests/fixtures");
    const auto level = simple_platformer::composeGameLevel(catalog, 1, 7);
    const auto items = simple_platformer::loadItemCatalog("tests/fixtures/items.json");
    REQUIRE(level.world.pickups().size() == 2);
    REQUIRE(
        level.world.pickups()[0].stack.item ==
        simple_platformer::itemDefinition(items, "medicine").id);
    REQUIRE(
        level.world.pickups()[1].stack.item == simple_platformer::itemDefinition(items, "key").id);
    REQUIRE(level.world.pickups()[1].body.bounds.size == glm::vec2{10, 12});
    const auto& exit = level.world.exit();
    if (!exit || !exit->requirement)
    {
        throw std::logic_error("Missing fixture exit requirement");
    }
    REQUIRE(exit->requirement->item == simple_platformer::itemDefinition(items, "key").id);
}

TEST_CASE("Exit item references resolve through the item catalogue", "[app][pickups]")
{
    const auto catalog = simple_platformer::parseLevelCatalog(
        R"({"startLevel":1,"levels":[{"number":1,"file":"unknown_item.json"}]})",
        "fixture",
        "tests/fixtures");
    REQUIRE_THROWS_WITH(
        simple_platformer::composeGameLevel(catalog, 1, 0),
        Catch::Matchers::ContainsSubstring("unknown_item.json:"));
    REQUIRE_THROWS_WITH(
        simple_platformer::composeGameLevel(catalog, 1, 0),
        Catch::Matchers::ContainsSubstring("requirement.item: unknown item 'missing'"));
}

TEST_CASE("Unknown unused pickup legend references identify their source", "[app][pickups]")
{
    const auto catalog = simple_platformer::parseLevelCatalog(
        R"({"startLevel":1,"levels":[{"number":1,"file":"unknown_pickup.json"}]})",
        "fixture",
        "tests/fixtures");
    REQUIRE_THROWS_WITH(
        simple_platformer::composeGameLevel(catalog, 1, 0),
        Catch::Matchers::ContainsSubstring(
            "unknown_pickup.json: objectLegend.K.definition: unknown pickup definition 'missing'"));
}

TEST_CASE("Level composition reuses the supplied session item catalogue", "[app][pickups]")
{
    const auto levels = simple_platformer::loadLevelCatalog("tests/fixtures/levels.json");
    // This session has an extra item before key, so its generated IDs differ from the file.
    const auto items = simple_platformer::parseItemCatalog(
        R"({"items":{
        "aaa":{"name":"Extra item","icon":{"position":[0,0],"size":[8,8]},"maximumStack":1},
        "key":{"name":"Session key","icon":{"position":[8,0],"size":[8,8]},"maximumStack":2},
        "medicine":{"name":"Medicine","icon":{"position":[16,0],"size":[8,8]},"maximumStack":3}
    }})",
        "session items");
    auto catalogs = simple_platformer::loadGameCatalogs(levels.levelDirectory);
    catalogs.items = items;
    const auto keyId = simple_platformer::itemDefinition(items, "key").id;
    const auto first = simple_platformer::composeGameLevel(levels, 10, 0, catalogs);
    const auto second = simple_platformer::composeGameLevel(levels, 25, 0, catalogs);
    REQUIRE(first.world.pickups().front().stack.item == keyId);
    REQUIRE(first.world.itemDefinition(keyId).name == "Session key");
    REQUIRE(second.world.itemDefinition(keyId).name == "Session key");
}

TEST_CASE("Level exit placement combines a definition with completion settings", "[app][exits]")
{
    const auto catalog = simple_platformer::loadLevelCatalog("tests/fixtures/levels.json");
    const auto level = simple_platformer::composeGameLevel(catalog, 10, 7);
    const auto& exit = level.world.exit();
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
    const auto catalog = simple_platformer::parseLevelCatalog(
        R"({"startLevel":1,"levels":[{"number":1,"file":"unknown_exit.json"}]})",
        "fixture",
        "tests/fixtures");
    REQUIRE_THROWS_WITH(
        simple_platformer::composeGameLevel(catalog, 1, 0),
        Catch::Matchers::ContainsSubstring(
            "unknown_exit.json: objectLegend.E.definition: unknown exit definition 'missing'"));
}
