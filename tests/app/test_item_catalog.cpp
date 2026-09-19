#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <nlohmann/json.hpp>
#include <limits>
#include <stdexcept>
#include "game/item_catalog.hpp"
#include "simple_platformer/inventory/item.hpp"

namespace
{
    nlohmann::json itemData()
    {
        return nlohmann::json::parse(R"({"items":{"herb":{
            "name":"Healing herb","maximumStack":4,
            "icon":{"position":[12,8],"size":[8,12]},"effect":"heal","effectAmount":3
        }}})");
    }
}

TEST_CASE("Item JSON resolves custom names to stable runtime IDs", "[app][items][json]")
{
    const auto catalog = simple_platformer::parseItemCatalog(itemData().dump(), "items.json");
    const auto stack = simple_platformer::resolveItemStack(catalog, {"herb", 2});
    REQUIRE(stack.item > 0);
    REQUIRE(stack.item == simple_platformer::itemDefinition(catalog, "herb").id);
    REQUIRE(stack.quantity == 2);
    const auto items = simple_platformer::composeItems(catalog, 6);
    REQUIRE(items.size() == 1);
    REQUIRE(items[0].name == "Healing herb");
    REQUIRE(items[0].maximumStack == 4);
    REQUIRE(items[0].effect == simple_platformer::ItemEffect::Heal);
    REQUIRE(items[0].effectAmount == 3);
    REQUIRE(items[0].icon.textureId == 6);
    REQUIRE(items[0].icon.size == glm::vec2{8, 12});
    REQUIRE_THROWS_AS(
        simple_platformer::resolveItemStack(catalog, {"missing", 1}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::resolveItemStack(catalog, {"herb", 0}), std::invalid_argument);
}

TEST_CASE("Item JSON rejects malformed and invalid definitions", "[app][items][json]")
{
    auto root = itemData();
    auto& item = root["items"]["herb"];
    SECTION("Authored IDs are not supported")
    {
        item["id"] = 17;
    }
    SECTION("Zero capacity")
    {
        item["maximumStack"] = 0;
    }
    SECTION("Unknown effect")
    {
        item["effect"] = "magic";
    }
    SECTION("No healing")
    {
        item["effectAmount"] = 0;
    }
    SECTION("No effect with amount")
    {
        item["effect"] = "none";
    }
    SECTION("Unknown field")
    {
        item["maximimStack"] = 1;
    }
    SECTION("Bad icon shape")
    {
        item["icon"]["size"] = {8};
    }
    SECTION("Negative source position")
    {
        item["icon"]["position"] = {-1, 0};
    }
    SECTION("Zero display size")
    {
        item["icon"]["displaySize"] = {0, 8};
    }
    SECTION("Wrong name type")
    {
        item["name"] = 1;
    }
    REQUIRE_THROWS_WITH(
        simple_platformer::parseItemCatalog(root.dump(), "items.json"),
        Catch::Matchers::ContainsSubstring("items.json: items."));
}

TEST_CASE("Item definitions are validated without JSON", "[app][items][validation]")
{
    auto catalog = simple_platformer::parseItemCatalog(itemData().dump(), "fixture");
    catalog.definitions.at("herb").icon.size.x = std::numeric_limits<float>::infinity();
    REQUIRE_THROWS_AS(simple_platformer::validateItemCatalog(catalog), std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::loadItemCatalog("tests/fixtures/levels/missing-items.json"),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::parseItemCatalog("not JSON", "broken"), std::invalid_argument);
}

TEST_CASE(
    "Item names receive distinct generated IDs even with matching display names",
    "[app][items][json]")
{
    auto root = itemData();
    root["items"]["other_herb"] = root["items"]["herb"];
    const auto catalog = simple_platformer::parseItemCatalog(root.dump(), "items.json");
    const auto& first = simple_platformer::itemDefinition(catalog, "herb");
    const auto& second = simple_platformer::itemDefinition(catalog, "other_herb");
    REQUIRE(first.id > 0);
    REQUIRE(second.id > 0);
    REQUIRE(first.id != second.id);
    REQUIRE(first.name == second.name);
}
