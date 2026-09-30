#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstddef>
#include <string>

#include <glm/vec2.hpp>

#include "content/game_catalogs.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "support/atlas_size.hpp"

namespace
{
    constexpr glm::ivec2 Atlas{256, 256};
    // Starts inside the atlas and runs four pixels past its right edge.
    constexpr simple_platformer::SpriteRegion PastTheEdge{{244.0F, 0.0F}, {16.0F, 16.0F}};
}

TEST_CASE("Catalog regions inside the atlas pass", "[app][content][atlas]")
{
    const simple_platformer::GameCatalogs catalogs =
        simple_platformer::loadGameCatalogs("tests/fixtures/catalogs", tests::AtlasSize);

    REQUIRE_NOTHROW(
        simple_platformer::validateAtlasRegions(catalogs, Atlas, "tests/fixtures/catalogs"));
}

TEST_CASE("A region past the atlas is reported by its file and field", "[app][content][atlas]")
{
    simple_platformer::GameCatalogs catalogs =
        simple_platformer::loadGameCatalogs("tests/fixtures/catalogs", tests::AtlasSize);
    std::string expected;
    SECTION("A tile sprite")
    {
        const auto& [name, id] = *catalogs.tiles.ids.rbegin();
        catalogs.tiles.definitions[static_cast<std::size_t>(id)].sprite = PastTheEdge;
        expected = "tiles.json: tiles." + name + ".sprite";
    }
    SECTION("An animation frame")
    {
        auto& [name, set] = *catalogs.animations.begin();
        set.clips.front().frames.back() = PastTheEdge;
        expected = "animations.json: animations." + name + ".idle.frames[" +
                   std::to_string(set.clips.front().frames.size() - 1) + "]";
    }
    SECTION("A projectile sprite")
    {
        auto& [name, definition] = *catalogs.actors.definitions.begin();
        definition.ranged = simple_platformer::RangedWeapon{};
        definition.ranged->projectileSprite.region = PastTheEdge;
        expected = "actors.json: actors." + name + ".ranged.sprite";
    }
    SECTION("An item icon")
    {
        auto& [name, definition] = *catalogs.items.definitions.begin();
        definition.icon.region = PastTheEdge;
        expected = "items.json: items." + name + ".icon";
    }
    SECTION("A pickup sprite")
    {
        auto& [name, definition] = *catalogs.pickups.begin();
        definition.sprite = simple_platformer::Sprite{0, PastTheEdge, {16.0F, 16.0F}};
        expected = "pickups.json: pickups." + name + ".sprite";
    }
    SECTION("An exit sprite")
    {
        auto& [name, definition] = *catalogs.exits.begin();
        definition.sprite.region = PastTheEdge;
        expected = "exits.json: exits." + name + ".sprite";
    }
    SECTION("A HUD icon")
    {
        catalogs.hudIcons.bag = PastTheEdge;
        expected = "hud.json: bag";
    }
    REQUIRE_THROWS_WITH(
        simple_platformer::validateAtlasRegions(catalogs, Atlas, "tests/fixtures/catalogs"),
        Catch::Matchers::ContainsSubstring(expected) &&
            Catch::Matchers::ContainsSubstring("runs past the 256 by 256 atlas"));
}
