#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <nlohmann/json.hpp>
#include <glm/vec2.hpp>
#include <string>
#include "content/hud_catalog.hpp"

namespace
{
    nlohmann::json hudData()
    {
        return nlohmann::json::parse(R"({
            "fullHeart": {"position": [96, 192], "size": [16, 16]},
            "emptyHeart": {"position": [112, 192], "size": [16, 16]},
            "bag": {"position": [64, 216], "size": [12, 14]}
        })");
    }
}

TEST_CASE("HUD icons name their atlas regions", "[app][content][hud]")
{
    const simple_platformer::HudIcons icons =
        simple_platformer::parseHudIcons(hudData().dump(), "hud.json");

    REQUIRE(icons.fullHeart.position == glm::vec2{96.0F, 192.0F});
    REQUIRE(icons.emptyHeart.position == glm::vec2{112.0F, 192.0F});
    REQUIRE(icons.bag.position == glm::vec2{64.0F, 216.0F});
    REQUIRE(icons.bag.size == glm::vec2{12.0F, 14.0F});
}

TEST_CASE("HUD icons load from a catalog file", "[app][content][hud]")
{
    const simple_platformer::HudIcons icons =
        simple_platformer::loadHudIcons("tests/fixtures/catalogs/hud.json");

    REQUIRE(icons.fullHeart.size == glm::vec2{16.0F, 16.0F});
}

TEST_CASE("HUD icons reject missing, unknown, and unusable regions", "[app][content][hud]")
{
    nlohmann::json data = hudData();
    std::string expected;
    SECTION("A missing icon")
    {
        data.erase("bag");
        expected = "bag";
    }
    SECTION("An unknown icon")
    {
        data["coin"] = data["bag"];
        expected = "coin";
    }
    SECTION("A negative position")
    {
        data["fullHeart"]["position"] = {-1, 192};
        expected = "fullHeart";
    }
    SECTION("An empty size")
    {
        data["emptyHeart"]["size"] = {16, 0};
        expected = "emptyHeart";
    }
    REQUIRE_THROWS_WITH(
        simple_platformer::parseHudIcons(data.dump(), "hud.json"),
        Catch::Matchers::ContainsSubstring("hud.json") &&
            Catch::Matchers::ContainsSubstring(expected));
}
