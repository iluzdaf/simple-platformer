#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include <limits>
#include <optional>
#include <stdexcept>

#include "game/content_validation.hpp"
#include "game/tile_catalog.hpp"
#include "game/example_level_data.hpp"
#include "simple_platformer/inventory/item.hpp"

TEST_CASE("Pickup and exit settings are validated without JSON", "[app][content][validation]")
{
    simple_platformer::ExamplePickupPlacement pickup;
    REQUIRE_NOTHROW(simple_platformer::validatePickupSettings(pickup));
    pickup.stack.quantity = 0;
    REQUIRE_THROWS_WITH(
        simple_platformer::validatePickupSettings(pickup, "objectLegend.K"),
        "objectLegend.K.quantity: expected a positive integer, got 0");
    pickup.stack.quantity = -2;
    REQUIRE_THROWS_AS(simple_platformer::validatePickupSettings(pickup), std::invalid_argument);

    simple_platformer::ExampleExitPlacement exit;
    REQUIRE_NOTHROW(simple_platformer::validateExitSettings(exit));
    exit.requirement = simple_platformer::ItemStack{1, 1};
    exit.nextLevel = 2;
    REQUIRE_NOTHROW(simple_platformer::validateExitSettings(exit));
    exit.requirement->quantity = 0;
    REQUIRE_THROWS_WITH(
        simple_platformer::validateExitSettings(exit),
        "exit.requirement.quantity: expected a positive integer, got 0");
    exit.requirement->quantity = -1;
    REQUIRE_THROWS_AS(simple_platformer::validateExitSettings(exit), std::invalid_argument);
    exit.requirement.reset();
    exit.nextLevel = 0;
    REQUIRE_THROWS_WITH(
        simple_platformer::validateExitSettings(exit),
        "exit.nextLevel: level number must be positive");
    exit.nextLevel = -1;
    REQUIRE_THROWS_AS(simple_platformer::validateExitSettings(exit), std::invalid_argument);
}

TEST_CASE("Unique placement validation retains authoring origins", "[app][content][validation]")
{
    REQUIRE_NOTHROW(simple_platformer::validateSinglePlacement({{"map[0][1]", 'P'}}, "player"));
    REQUIRE_THROWS_WITH(
        simple_platformer::validateSinglePlacement({}, "player"),
        "player: expected exactly one placement");
    REQUIRE_THROWS_WITH(
        simple_platformer::validateSinglePlacement({}, "exit"),
        "exit: expected exactly one placement");
    REQUIRE_THROWS_WITH(
        simple_platformer::validateSinglePlacement(
            {{"map[0][1]", 'P'}, {"map[0][2]", 'P'}}, "player"),
        "map[0][2]: second player marker 'P'; player already placed at map[0][1]");
    REQUIRE_THROWS_WITH(
        simple_platformer::validateSinglePlacement(
            {{"exit", std::nullopt}, {"map[0][3]", 'E'}}, "exit"),
        "map[0][3]: second exit marker 'E'; exit already placed at exit");
    REQUIRE_THROWS_WITH(
        simple_platformer::validateSinglePlacement(
            {{"playerSpawnCell", std::nullopt}, {"playerSpawnFeet", std::nullopt}}, "player"),
        "playerSpawnFeet: second player placement; player already placed at playerSpawnCell");
}

TEST_CASE(
    "Tile catalogue validation accepts C++ definitions without JSON",
    "[app][content][validation]")
{
    const simple_platformer::TileCatalog catalog{
        {{false, false, {}}, {true, false, {{0, 0}, {16, 16}}}}, {{"empty", 0}, {"glass", 1}}};
    REQUIRE_NOTHROW(simple_platformer::validateTileCatalog(catalog));
    REQUIRE_NOTHROW(
        simple_platformer::validateTileLegend({{'.', "empty"}, {'X', "glass"}}, catalog));
    REQUIRE_THROWS_AS(
        simple_platformer::validateTileLegend({{'?', "missing"}}, catalog), std::invalid_argument);
}

TEST_CASE("Tile catalogue validation rejects invalid C++ definitions", "[app][content][validation]")
{
    simple_platformer::TileCatalog catalog{
        {{false, false, {}}, {true, false, {{0, 0}, {16, 16}}}}, {{"empty", 0}, {"glass", 1}}};
    SECTION("Missing empty")
    {
        catalog.ids.erase("empty");
    }
    SECTION("Empty has wrong ID")
    {
        catalog.ids["empty"] = 1;
    }
    SECTION("Empty blocks movement")
    {
        catalog.definitions[0].blocksMovement = true;
    }
    SECTION("Empty blocks sight")
    {
        catalog.definitions[0].blocksSight = true;
    }
    SECTION("Negative ID")
    {
        catalog.ids["glass"] = -1;
    }
    SECTION("Out of range ID")
    {
        catalog.ids["glass"] = 2;
    }
    SECTION("Repeated ID")
    {
        catalog.ids["alias"] = 1;
    }
    SECTION("Unnamed definition")
    {
        catalog.ids.erase("glass");
    }
    SECTION("Negative sprite position")
    {
        catalog.definitions[1].sprite.position.x = -1;
    }
    SECTION("Zero sprite size")
    {
        catalog.definitions[1].sprite.size.x = 0;
    }
    SECTION("Nonfinite sprite")
    {
        catalog.definitions[1].sprite.size.y = std::numeric_limits<float>::infinity();
    }
    REQUIRE_THROWS_AS(simple_platformer::validateTileCatalog(catalog), std::invalid_argument);
}

TEST_CASE("Legend symbols are unambiguous independently of JSON", "[app][content][validation]")
{
    REQUIRE_NOTHROW(simple_platformer::validateLegendSymbols({".", "#"}, {"P", "Z"}));
    REQUIRE_THROWS_AS(
        simple_platformer::validateLegendSymbols({"long"}, {}), std::invalid_argument);
    REQUIRE_THROWS_AS(simple_platformer::validateLegendSymbols({"."}, {""}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::validateLegendSymbols({"."}, {"."}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::validateLegendSymbols({".", "."}, {}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::validateLegendSymbols({"."}, {"P", "P"}), std::invalid_argument);
}

TEST_CASE(
    "Map authoring validation reports useful paths without JSON",
    "[app][content][validation]")
{
    REQUIRE_NOTHROW(simple_platformer::validateMapRows({"..", ".."}, {{'.', "empty"}}));
    REQUIRE_THROWS_WITH(
        simple_platformer::validateMapRows({}, {{'.', "empty"}}), "map: expected at least one row");
    REQUIRE_THROWS_WITH(
        simple_platformer::validateMapRows({""}, {{'.', "empty"}}), "map[0]: row cannot be empty");
    REQUIRE_THROWS_WITH(
        simple_platformer::validateMapRows({"..", "."}, {{'.', "empty"}}),
        "map[1]: expected 2 columns, got 1");
    REQUIRE_THROWS_WITH(
        simple_platformer::validateMapRows({".?"}, {{'.', "empty"}}),
        "map[0][1]: unknown symbol '?'; define it in tileLegend or objectLegend");
}
