#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <nlohmann/json.hpp>
#include <limits>
#include <stdexcept>
#include "game/exit_catalog.hpp"
#include "game/example_content.hpp"
#include "game/level_catalog.hpp"
#include "game/content_validation.hpp"
#include "game/example_level_data.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/render/sprite.hpp"

namespace
{
    nlohmann::json exitData()
    {
        return nlohmann::json::parse(R"({"exits":{"gate":{
            "bodySize":[12,24],"sprite":{"position":[8,16],"size":[8,12],"displaySize":[16,24],"anchor":"center"}
        }}})");
    }
}

TEST_CASE("Exit definitions compose independent bounds and sprites", "[app][exits]")
{
    const auto catalog = simple_platformer::parseExitCatalog(exitData().dump(), "exits.json");
    const auto exit = simple_platformer::composeExit(
        simple_platformer::exitDefinition(catalog, "gate"), 7, {40, 48});
    REQUIRE(exit.bounds.size == glm::vec2{12, 24});
    REQUIRE(simple_platformer::feetOf(exit.bounds) == glm::vec2{40, 48});
    if (!exit.sprite)
    {
        throw std::logic_error("Missing exit sprite");
    }
    REQUIRE(exit.sprite->textureId == 7);
    REQUIRE(exit.sprite->size == glm::vec2{16, 24});
    REQUIRE(exit.sprite->anchor == simple_platformer::SpriteAnchor::BodyCenter);
    REQUIRE_FALSE(exit.requirement.has_value());
    REQUIRE_FALSE(exit.nextLevel.has_value());
    REQUIRE_FALSE(exit.consumeItem);
}

TEST_CASE(
    "Exit catalogue validates unused definitions and rejects placement settings",
    "[app][exits][json]")
{
    auto root = exitData();
    auto& definition = root["exits"]["gate"];
    SECTION("Invalid bounds")
    {
        definition["bodySize"] = {0, 24};
    }
    SECTION("Invalid sprite")
    {
        definition["sprite"]["size"] = {-1, 12};
    }
    SECTION("Missing sprite")
    {
        definition.erase("sprite");
    }
    SECTION("Missing bounds")
    {
        definition.erase("bodySize");
    }
    SECTION("Malformed vector")
    {
        definition["bodySize"] = {12};
    }
    SECTION("Destination belongs to placement")
    {
        definition["nextLevel"] = 2;
    }
    SECTION("Requirement belongs to placement")
    {
        definition["requirement"] = {{"item", "key"}, {"quantity", 1}};
    }
    SECTION("Consumption belongs to placement")
    {
        definition["consumeItem"] = true;
    }
    REQUIRE_THROWS_WITH(
        simple_platformer::parseExitCatalog(root.dump(), "exits.json"),
        Catch::Matchers::ContainsSubstring("exits.json: exits.gate:"));
}

TEST_CASE("Exit definitions and placement names validate without JSON", "[app][exits][validation]")
{
    auto catalog = simple_platformer::parseExitCatalog(exitData().dump(), "fixture");
    catalog.at("gate").bodySize.x = std::numeric_limits<float>::infinity();
    REQUIRE_THROWS_AS(simple_platformer::validateExitCatalog(catalog), std::invalid_argument);
    simple_platformer::ExampleExitPlacement placement;
    REQUIRE_THROWS_WITH(
        simple_platformer::validateExitSettings(placement),
        "exit.definition: exit definition name cannot be empty");
    REQUIRE_THROWS_AS(
        simple_platformer::loadExitCatalog("tests/fixtures/levels/missing-exits.json"),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::parseExitCatalog("not JSON", "broken"), std::invalid_argument);
}

TEST_CASE("Level exit placement combines a definition with completion settings", "[app][exits]")
{
    const auto catalog = simple_platformer::loadLevelCatalog("tests/fixtures/levels/levels.json");
    const auto level = simple_platformer::makeGameLevel(catalog, 10, 7);
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
        "tests/fixtures/levels");
    REQUIRE_THROWS_WITH(
        simple_platformer::makeGameLevel(catalog, 1, 0),
        Catch::Matchers::ContainsSubstring(
            "unknown_exit.json: objectLegend.E.definition: unknown exit definition 'missing'"));
}
