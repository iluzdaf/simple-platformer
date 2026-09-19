#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <nlohmann/json.hpp>
#include <limits>
#include <stdexcept>
#include "content/animation_catalog.hpp"
#include "content/actor_catalog.hpp"
#include "content/actor_definition.hpp"
#include "content/content_json.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/sprite.hpp"

TEST_CASE("Animation JSON preserves frame order timing and looping", "[app][animations]")
{
    auto root = nlohmann::json::parse(
        simple_platformer::loadContentText("tests/fixtures/levels/animations.json"));
    auto& move = root["animations"]["test_actor"]["move"];
    move["frames"].push_back({{"position", {8, 0}}, {"size", {8, 12}}});
    move["frames"].push_back({{"position", {0, 0}}, {"size", {8, 12}}});
    move["frameDuration"] = 0.25;
    const auto catalog = simple_platformer::parseAnimationCatalog(root.dump(), "test animations");
    const auto& set = simple_platformer::animationSet(catalog, "test_actor");
    const auto& clip = simple_platformer::clipFor(set, simple_platformer::AnimationName::Move);
    REQUIRE(clip.frames.size() == 3);
    REQUIRE(clip.frameDuration == 0.25F);
    REQUIRE(clip.looping);
    REQUIRE(simple_platformer::frameAt(clip, 0.25F).position.x == 8);
    REQUIRE(simple_platformer::frameAt(clip, 0.5F).position.x == 0);
    REQUIRE_FALSE(simple_platformer::clipFor(set, simple_platformer::AnimationName::Death).looping);
}

TEST_CASE("Animation catalogues reject invalid content with source context", "[app][animations]")
{
    auto root = nlohmann::json::parse(
        simple_platformer::loadContentText("tests/fixtures/levels/animations.json"));
    auto& set = root["animations"]["test_actor"];
    SECTION("Missing clip")
    {
        set.erase("death");
    }
    SECTION("Unknown clip")
    {
        set["run"] = set["move"];
    }
    SECTION("Empty frames")
    {
        set["idle"]["frames"] = nlohmann::json::array();
    }
    SECTION("Zero duration")
    {
        set["idle"]["frameDuration"] = 0;
    }
    SECTION("Boolean duration")
    {
        set["idle"]["frameDuration"] = true;
    }
    SECTION("Nonboolean looping")
    {
        set["idle"]["looping"] = 1;
    }
    SECTION("Invalid rectangle")
    {
        set["idle"]["frames"][0]["size"] = {0, 12};
    }
    SECTION("Negative position")
    {
        set["idle"]["frames"][0]["position"] = {-1, 0};
    }
    SECTION("Vector shape")
    {
        set["idle"]["frames"][0]["position"] = {1};
    }
    SECTION("Mixed sizes")
    {
        set["move"]["frames"][0]["size"] = {16, 12};
    }
    SECTION("Unknown field")
    {
        set["idle"]["elapsed"] = 0;
    }
    REQUIRE_THROWS_WITH(
        simple_platformer::parseAnimationCatalog(root.dump(), "clips.json"),
        Catch::Matchers::ContainsSubstring("clips.json: animations.test_actor"));
}

TEST_CASE("Animation validation also accepts C++ definitions", "[app][animations]")
{
    auto catalog = simple_platformer::loadAnimationCatalog("tests/fixtures/levels/animations.json");
    SECTION("Nonfinite timing")
    {
        catalog.at("test_actor").clips.front().frameDuration =
            std::numeric_limits<float>::infinity();
    }
    SECTION("Duplicate clip")
    {
        auto& set = catalog.at("test_actor");
        set.clips.push_back(set.clips.front());
    }
    SECTION("Empty name")
    {
        catalog.emplace("", catalog.at("test_actor"));
    }
    REQUIRE_THROWS_AS(simple_platformer::validateAnimationCatalog(catalog), std::invalid_argument);
}

TEST_CASE("Animation loading reports missing files and unknown sets", "[app][animations]")
{
    REQUIRE_THROWS_AS(
        simple_platformer::loadAnimationCatalog("tests/fixtures/missing-animations.json"),
        std::invalid_argument);
    REQUIRE_THROWS_WITH(
        simple_platformer::parseAnimationCatalog("{", "broken.json"),
        Catch::Matchers::ContainsSubstring("broken.json:"));
    REQUIRE_THROWS_WITH(
        simple_platformer::animationSet({}, "missing"),
        Catch::Matchers::ContainsSubstring("unknown animation set 'missing'"));
}

TEST_CASE("Animation domain diagnostics identify the clip and frame", "[app][animations]")
{
    auto root = nlohmann::json::parse(
        simple_platformer::loadContentText("tests/fixtures/levels/animations.json"));
    SECTION("Duration")
    {
        root["animations"]["test_actor"]["move"]["frameDuration"] = 0;
        REQUIRE_THROWS_WITH(
            simple_platformer::parseAnimationCatalog(root.dump(), "clips.json"),
            Catch::Matchers::ContainsSubstring("animations.test_actor: move.frameDuration:"));
    }
    SECTION("Rectangle")
    {
        root["animations"]["test_actor"]["move"]["frames"][0]["size"] = {0, 12};
        REQUIRE_THROWS_WITH(
            simple_platformer::parseAnimationCatalog(root.dump(), "clips.json"),
            Catch::Matchers::ContainsSubstring("animations.test_actor: move.frames[0]:"));
    }
}

TEST_CASE("Actors have independent playback of shared animation definitions", "[app][animations]")
{
    const auto animations =
        simple_platformer::loadAnimationCatalog("tests/fixtures/levels/animations.json");
    const auto actors = simple_platformer::parseActorCatalog(
        R"({"player":"hero","actors":{"hero":{"platformer":{},"health":3,"inventorySlots":2,"animations":"test_actor"}}})",
        "actors.json",
        animations);
    const auto& definition = simple_platformer::actorDefinition(actors, "hero");
    auto first = simple_platformer::composeActor(definition, animations, 7);
    const auto second = simple_platformer::composeActor(definition, animations, 7);
    if (!first.animator || !second.animator || !first.sprite)
    {
        throw std::logic_error("Missing animation components");
    }
    simple_platformer::updateAnimation(
        *first.animator, *first.sprite, simple_platformer::AnimationName::Idle, 0.1F);
    REQUIRE(first.animator->elapsed == 0.1F);
    REQUIRE(second.animator->elapsed == 0);
    REQUIRE(first.sprite->textureId == 7);
    REQUIRE(first.sprite->region.size == glm::vec2{8, 12});
}
