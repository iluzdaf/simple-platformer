#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <nlohmann/json.hpp>
#include <limits>
#include <stdexcept>
#include "content/item_catalog.hpp"
#include "content/pickup_catalog.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/render/sprite.hpp"

TEST_CASE("Pickup definitions compose bounds and optional world sprites", "[app][pickups]")
{
    const auto items = simple_platformer::loadItemCatalog("tests/fixtures/catalogs/items.json");
    const auto catalog =
        simple_platformer::loadPickupCatalog("tests/fixtures/catalogs/pickups.json", items);
    const auto key = simple_platformer::composePickup(
        simple_platformer::pickupDefinition(catalog, "door_key"), items, 7, {40, 48});
    REQUIRE(key.stack.item == simple_platformer::itemDefinition(items, "key").id);
    REQUIRE(key.stack.quantity == 1);
    REQUIRE(key.body.bounds.size == glm::vec2{10, 12});
    REQUIRE(simple_platformer::feetOf(key.body.bounds) == glm::vec2{40, 48});
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
    const auto items = simple_platformer::loadItemCatalog("tests/fixtures/catalogs/items.json");
    auto pickupJson = nlohmann::json::parse(
        R"({"pickups":{"unused":{"item":"key","quantity":1,"bodySize":[10,12]}}})");
    auto& definition = pickupJson["pickups"]["unused"];
    SECTION("Missing body size")
    {
        definition.erase("bodySize");
    }
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
        simple_platformer::parsePickupCatalog(pickupJson.dump(), "pickups.json", items),
        Catch::Matchers::ContainsSubstring("pickups.json: pickups.unused"));
}

TEST_CASE("Pickup definitions reject invalid C++ data without JSON", "[app][pickups][validation]")
{
    const auto items = simple_platformer::loadItemCatalog("tests/fixtures/catalogs/items.json");
    simple_platformer::PickupDefinition definition;
    definition.stack = {"key", 1};
    definition.bodySize = {10.0F, 12.0F};
    REQUIRE_NOTHROW(simple_platformer::validatePickupDefinition(definition, items));
    definition.bodySize.x = std::numeric_limits<float>::quiet_NaN();
    REQUIRE_THROWS_AS(
        simple_platformer::validatePickupDefinition(definition, items), std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::loadPickupCatalog("tests/fixtures/catalogs/missing-pickups.json", items),
        std::invalid_argument);
}
