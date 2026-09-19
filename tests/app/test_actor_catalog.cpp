#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include "game/actor_catalog.hpp"
#include "game/actor_definition.hpp"
#include "game/example_content.hpp"
#include "game/level_catalog.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/render/sprite.hpp"

TEST_CASE("Actor catalogue references compose through level loading", "[app][actors]")
{
    const auto levels = simple_platformer::parseLevelCatalog(
        R"({"startLevel":1,"levels":[{"number":1,"file":"actor_placement.json"}]})",
        "fixture",
        "tests/fixtures/levels");
    const auto level = simple_platformer::makeGameLevel(levels, 1, 0);
    REQUIRE(level.world.actors().size() == 1);
    const auto& actor = level.world.actors().front();
    if (!actor.platformerMovement)
    {
        throw std::logic_error("Guard has no movement");
    }
    REQUIRE(actor.platformerMovement->config.maximumSpeed == 23);
    REQUIRE(simple_platformer::feetOf(actor.body.bounds).x == 56);
    const auto invalid = simple_platformer::parseLevelCatalog(
        R"({"startLevel":1,"levels":[{"number":1,"file":"unknown_actor.json"}]})",
        "fixture",
        "tests/fixtures/levels");
    REQUIRE_THROWS_WITH(
        simple_platformer::makeGameLevel(invalid, 1, 0),
        Catch::Matchers::ContainsSubstring(
            "objectLegend.Z.definition: unknown actor definition 'missing'"));
}

TEST_CASE("Ranged definitions create fresh weapons with runtime texture IDs", "[app][actors]")
{
    const auto catalog = simple_platformer::parseActorCatalog(
        R"({
      "player":"hero", "actors":{"hero":{"health":4,"inventorySlots":2,
      "platformer":{}, "team":"player", "ranged":{"damage":2,"projectileSize":[3,2],
      "projectileSpeed":120,"projectileLifetime":0.6,"shootDuration":0.2,"recoveryDuration":0.8,
      "spritePosition":[4,8],"spriteSize":[8,4]}}}})",
        "weapons");
    auto definition = simple_platformer::actorDefinition(catalog, "hero");
    if (!definition.ranged)
    {
        throw std::logic_error("Weapon was not parsed");
    }
    definition.ranged->phase = simple_platformer::RangedPhase::Recovery;
    definition.ranged->firedThisUpdate = true;
    const auto actor = simple_platformer::composeActor(definition, 9);
    if (!actor.rangedWeapon)
    {
        throw std::logic_error("Weapon was not composed");
    }
    REQUIRE(actor.rangedWeapon->damage == 2);
    REQUIRE(actor.rangedWeapon->projectileSpeed == 120);
    REQUIRE(actor.rangedWeapon->projectileSprite.textureId == 9);
    REQUIRE(actor.rangedWeapon->projectileSprite.region.position.x == 4);
    REQUIRE(actor.rangedWeapon->phase == simple_platformer::RangedPhase::Ready);
    REQUIRE_FALSE(actor.rangedWeapon->firedThisUpdate);
}

TEST_CASE("Actor composition creates fresh independent runtime state", "[app][actors]")
{
    simple_platformer::ActorDefinition definition;
    definition.platformer = simple_platformer::PlatformerMovementConfig{};
    definition.platformer.value().maximumSpeed = 42;
    definition.team = simple_platformer::Team::Enemy;
    definition.senses = simple_platformer::NpcSenses{70, 2};
    definition.health = 4;
    definition.inventorySlots = 2;
    definition.bite = simple_platformer::BiteAttack{};
    definition.bite.value().phase = simple_platformer::BitePhase::Recovery;
    definition.bite.value().phaseTimeRemaining = 10;
    const auto first = simple_platformer::composeActor(
        definition, 0, {24, 32}, simple_platformer::Patrol{{8, 32}, {40, 32}, true});
    auto second = simple_platformer::composeActor(definition, 0, {40, 32});
    if (!first.platformerMovement || !first.bite || !first.health || !first.inventory ||
        !second.health)
    {
        throw std::logic_error("Composition omitted a requested component");
    }
    REQUIRE(first.platformerMovement.value().config.maximumSpeed == 42);
    REQUIRE(simple_platformer::feetOf(first.body.bounds).x == 24);
    REQUIRE(first.brain.has_value());
    REQUIRE(first.pathFollower.has_value());
    REQUIRE(first.patrol.has_value());
    REQUIRE_FALSE(second.patrol.has_value());
    REQUIRE(first.bite.value().phase == simple_platformer::BitePhase::Ready);
    REQUIRE(first.bite.value().phaseTimeRemaining == 0);
    second.health.value().current = 1;
    REQUIRE(first.health.value().current == 4);
    REQUIRE(first.inventory.value().slots().size() == 2);
}

TEST_CASE("Actor definitions reuse engine component validation", "[app][actors]")
{
    simple_platformer::ActorDefinition definition;
    definition.platformer = simple_platformer::PlatformerMovementConfig{};
    SECTION("Two movements")
    {
        definition.flying = simple_platformer::FlyingMovement{};
    }
    SECTION("Invalid body")
    {
        definition.bodySize.x = 0;
    }
    SECTION("Invalid health")
    {
        definition.health = 0;
    }
    SECTION("Invalid senses")
    {
        definition.senses = simple_platformer::NpcSenses{-1, 1};
    }
    SECTION("Negative movement")
    {
        definition.platformer.value().maximumSpeed = -1;
    }
    SECTION("Invalid inventory")
    {
        definition.inventorySlots = 0;
    }
    SECTION("Neutral attacker")
    {
        definition.bite = simple_platformer::BiteAttack{};
    }
    REQUIRE_THROWS_AS(
        simple_platformer::validateActorDefinition(definition), std::invalid_argument);
}

TEST_CASE("Actor JSON accepts custom names and configures component choices", "[app][actors][json]")
{
    const auto catalog = simple_platformer::parseActorCatalog(
        R"({
        "player":"hero", "actors":{
          "hero":{"platformer":{"jumpSpeed":210},"health":5,"inventorySlots":3},
          "scout":{"flying":{"speed":25},"team":"enemy","senses":{"noticeDistance":40},
                   "bodySize":[8,6],"animations":"bat","spriteAnchor":"center","bite":{"damage":2}}
        }})",
        "test actors");
    REQUIRE(catalog.player == "hero");
    const auto actor =
        simple_platformer::composeActor(simple_platformer::actorDefinition(catalog, "scout"), 7);
    if (!actor.flyingMovement || !actor.bite || !actor.sprite)
    {
        throw std::logic_error("Composition omitted scout components");
    }
    REQUIRE(actor.flyingMovement.value().speed == 25);
    REQUIRE(actor.bite.value().damage == 2);
    REQUIRE(actor.sprite.value().textureId == 7);
    REQUIRE(actor.sprite.value().anchor == simple_platformer::SpriteAnchor::BodyCenter);
    REQUIRE_FALSE(actor.platformerMovement.has_value());
    REQUIRE_THROWS_AS(
        simple_platformer::actorDefinition(catalog, "missing"), std::invalid_argument);
}

TEST_CASE(
    "Actor JSON rejects malformed and invalid definitions including unused ones",
    "[app][actors][json]")
{
    auto root = nlohmann::json::parse(
        R"({"player":"hero","actors":{"hero":{"platformer":{},"health":3,"inventorySlots":2}}})");
    SECTION("Missing player reference")
    {
        root["player"] = "missing";
    }
    SECTION("Unknown animation")
    {
        root["actors"]["hero"]["animations"] = "missing";
    }
    SECTION("Fractional health")
    {
        root["actors"]["hero"]["health"] = 1.5;
    }
    SECTION("Boolean speed")
    {
        root["actors"]["hero"]["platformer"]["maximumSpeed"] = true;
    }
    SECTION("Unknown field")
    {
        root["actors"]["hero"]["heath"] = 3;
    }
    SECTION("Runtime state")
    {
        root["actors"]["hero"]["brain"] = {};
    }
    SECTION("Unused definition")
    {
        root["actors"]["unused"] = {{"flying", {{"speed", -1}}}};
    }
    REQUIRE_THROWS_WITH(
        simple_platformer::parseActorCatalog(root.dump(), "actors.json"),
        Catch::Matchers::ContainsSubstring("actors.json:"));
}
