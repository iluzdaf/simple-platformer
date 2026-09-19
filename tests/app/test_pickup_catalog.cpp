#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <nlohmann/json.hpp>
#include <limits>
#include <stdexcept>
#include "game/item_catalog.hpp"
#include "game/pickup_catalog.hpp"
#include "game/example_content.hpp"
#include "game/level_catalog.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/render/sprite.hpp"

TEST_CASE("Pickup definitions compose bounds and optional world sprites", "[app][pickups]")
{
    const auto items = simple_platformer::loadItemCatalog("tests/fixtures/levels/items.json");
    const auto catalog =
        simple_platformer::loadPickupCatalog("tests/fixtures/levels/pickups.json", items);
    const auto key = simple_platformer::composePickup(
        simple_platformer::pickupDefinition(catalog, "door_key"), items, 7, {40, 48});
    REQUIRE(key.stack.item == simple_platformer::itemDefinition(items, "key").id);
    REQUIRE(key.stack.quantity == 1);
    REQUIRE(key.bounds.size == glm::vec2{10, 12});
    REQUIRE(simple_platformer::feetOf(key.bounds) == glm::vec2{40, 48});
    REQUIRE_FALSE(key.sprite.has_value());
    const auto medicine = simple_platformer::composePickup(
        simple_platformer::pickupDefinition(catalog, "medicine_box"), items, 7, {24, 32});
    if (!medicine.sprite)
    {
        throw std::logic_error("Missing sprite override");
    }
    REQUIRE(medicine.sprite->textureId == 7);
    REQUIRE(medicine.sprite->size == glm::vec2{24, 16});
    REQUIRE(medicine.sprite->anchor == simple_platformer::SpriteAnchor::BodyCenter);
    REQUIRE(simple_platformer::itemDefinition(items, "medicine").icon.size == glm::vec2{8, 8});
}

TEST_CASE("Pickup JSON validates every definition including unused entries", "[app][pickups][json]")
{
    const auto items = simple_platformer::loadItemCatalog("tests/fixtures/levels/items.json");
    auto root = nlohmann::json::parse(R"({"pickups":{"unused":{"item":"key","quantity":1}}})");
    auto& definition = root["pickups"]["unused"];
    SECTION("Unknown item")
    {
        definition["item"] = "missing";
    }
    SECTION("Nonpositive quantity")
    {
        definition["quantity"] = 0;
    }
    SECTION("Fractional quantity")
    {
        definition["quantity"] = 0.5;
    }
    SECTION("Invalid bounds")
    {
        definition["bodySize"] = {0, 12};
    }
    SECTION("Wrong vector shape")
    {
        definition["bodySize"] = {12};
    }
    SECTION("Unknown field")
    {
        definition["bodySze"] = {12, 12};
    }
    SECTION("Invalid sprite")
    {
        definition["sprite"] = {{"position", {0, 0}}, {"size", {0, 8}}};
    }
    REQUIRE_THROWS_WITH(
        simple_platformer::parsePickupCatalog(root.dump(), "pickups.json", items),
        Catch::Matchers::ContainsSubstring("pickups.json: pickups.unused:"));
}

TEST_CASE("Pickup definitions reject invalid C++ data without JSON", "[app][pickups][validation]")
{
    const auto items = simple_platformer::loadItemCatalog("tests/fixtures/levels/items.json");
    simple_platformer::PickupDefinition definition;
    definition.stack = {"key", 1};
    REQUIRE_NOTHROW(simple_platformer::validatePickupDefinition(definition, items));
    definition.bodySize.x = std::numeric_limits<float>::quiet_NaN();
    REQUIRE_THROWS_AS(
        simple_platformer::validatePickupDefinition(definition, items), std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::loadPickupCatalog("tests/fixtures/levels/missing-pickups.json", items),
        std::invalid_argument);
}

TEST_CASE("Named pickups and exit requirements resolve through level composition", "[app][pickups]")
{
    const auto catalog = simple_platformer::parseLevelCatalog(
        R"({"startLevel":1,"levels":[{"number":1,"file":"pickup_placement.json"}]})",
        "fixture",
        "tests/fixtures/levels");
    const auto level = simple_platformer::makeGameLevel(catalog, 1, 7);
    const auto items = simple_platformer::loadItemCatalog("tests/fixtures/levels/items.json");
    REQUIRE(level.world.pickups().size() == 2);
    REQUIRE(
        level.world.pickups()[0].stack.item ==
        simple_platformer::itemDefinition(items, "medicine").id);
    REQUIRE(
        level.world.pickups()[1].stack.item == simple_platformer::itemDefinition(items, "key").id);
    REQUIRE(level.world.pickups()[1].bounds.size == glm::vec2{10, 12});
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
        "tests/fixtures/levels");
    REQUIRE_THROWS_WITH(
        simple_platformer::makeGameLevel(catalog, 1, 0),
        Catch::Matchers::ContainsSubstring("unknown_item.json:"));
    REQUIRE_THROWS_WITH(
        simple_platformer::makeGameLevel(catalog, 1, 0),
        Catch::Matchers::ContainsSubstring("requirement.item: unknown item 'missing'"));
}

TEST_CASE("Unknown unused pickup legend references identify their source", "[app][pickups]")
{
    const auto catalog = simple_platformer::parseLevelCatalog(
        R"({"startLevel":1,"levels":[{"number":1,"file":"unknown_pickup.json"}]})",
        "fixture",
        "tests/fixtures/levels");
    REQUIRE_THROWS_WITH(
        simple_platformer::makeGameLevel(catalog, 1, 0),
        Catch::Matchers::ContainsSubstring(
            "unknown_pickup.json: objectLegend.K.definition: unknown pickup definition 'missing'"));
}

TEST_CASE("Level composition reuses the supplied session item catalogue", "[app][pickups]")
{
    const auto levels = simple_platformer::loadLevelCatalog("tests/fixtures/levels/levels.json");
    // This session has an extra item before key, so its generated IDs differ from the file.
    const auto items = simple_platformer::parseItemCatalog(
        R"({"items":{
        "aaa":{"name":"Extra item","icon":{"position":[0,0],"size":[8,8]},"maximumStack":1},
        "key":{"name":"Session key","icon":{"position":[8,0],"size":[8,8]},"maximumStack":2},
        "medicine":{"name":"Medicine","icon":{"position":[16,0],"size":[8,8]},"maximumStack":3}
    }})",
        "session items");
    const auto keyId = simple_platformer::itemDefinition(items, "key").id;
    const auto first = simple_platformer::makeGameLevel(levels, 10, 0, items);
    const auto second = simple_platformer::makeGameLevel(levels, 25, 0, items);
    REQUIRE(first.world.pickups().front().stack.item == keyId);
    REQUIRE(first.world.itemDefinition(keyId).name == "Session key");
    REQUIRE(second.world.itemDefinition(keyId).name == "Session key");
}
