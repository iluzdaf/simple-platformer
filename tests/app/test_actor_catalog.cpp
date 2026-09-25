#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <optional>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include "content/actor_catalog.hpp"
#include "content/animation_catalog.hpp"
#include "content/machine_catalog.hpp"
#include "simple_platformer/npc/npc_state_machine.hpp"
#include "content/actor_definition.hpp"
#include "game/level_composition.hpp"
#include "content/level_catalog.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "support/actor_components.hpp"

TEST_CASE("Actor catalogue references compose through level loading", "[app][actors]")
{
    const auto levels = simple_platformer::parseLevelCatalog(
        R"({"startLevel":1,"levels":[{"number":1,"file":"actor_placement.json"}]})",
        "fixture",
        "tests/fixtures");
    auto level = simple_platformer::composeGameLevel(levels, 1, 0);
    REQUIRE(level.world.actors().size() == 1);
    auto& actor = level.world.actors().front();
    REQUIRE(tests::platformerMovement(actor).config.maximumSpeed == 23);
    REQUIRE(simple_platformer::feetOf(actor.body.bounds).x == 56);
    const auto invalid = simple_platformer::parseLevelCatalog(
        R"({"startLevel":1,"levels":[{"number":1,"file":"unknown_actor.json"}]})",
        "fixture",
        "tests/fixtures");
    REQUIRE_THROWS_WITH(
        simple_platformer::composeGameLevel(invalid, 1, 0),
        Catch::Matchers::ContainsSubstring(
            "objectLegend.Z.definition: unknown actor definition 'missing'"));
}

TEST_CASE("Ranged definitions create fresh weapons with runtime texture IDs", "[app][actors]")
{
    const auto catalog = simple_platformer::parseActorCatalog(
        R"({
      "player":"hero", "actors":{"hero":{"bodySize":[12,20],"health":4,"inventorySlots":2,
      "platformer":{}, "team":"player", "ranged":{"damage":2,"projectileSize":[3,2],
      "projectileSpeed":120,"projectileLifetime":0.6,"shootDuration":0.2,"recoveryDuration":0.8,
      "sprite": {"position": [4,8], "size": [8,4]}}}}})",
        "weapons",
        {});
    auto definition = simple_platformer::actorDefinition(catalog, "hero");
    if (!definition.ranged)
    {
        throw std::logic_error("Weapon was not parsed");
    }
    definition.ranged->phase = simple_platformer::RangedPhase::Recovery;
    definition.ranged->lastFiredTimeSeconds = 3.0F;
    auto actor = simple_platformer::composeActor(definition, {}, 9);
    REQUIRE(tests::rangedWeapon(actor).damage == 2);
    REQUIRE(tests::rangedWeapon(actor).projectileSpeed == 120);
    REQUIRE(tests::rangedWeapon(actor).projectileSprite.textureId == 9);
    REQUIRE(tests::rangedWeapon(actor).projectileSprite.region.position.x == 4);
    REQUIRE(tests::rangedWeapon(actor).phase == simple_platformer::RangedPhase::Ready);
    REQUIRE_FALSE(tests::rangedWeapon(actor).lastFiredTimeSeconds.has_value());
}

TEST_CASE("Actor composition creates fresh independent runtime state", "[app][actors]")
{
    simple_platformer::ActorDefinition definition;
    definition.bodySize = {12.0F, 20.0F};
    definition.platformer = simple_platformer::PlatformerMovementConfig{};
    definition.platformer.value().maximumSpeed = 42;
    definition.team = simple_platformer::Team::Enemy;
    definition.senses = simple_platformer::NpcSenses{70, 2};
    definition.health = 4;
    definition.inventorySlots = 2;
    definition.bite = simple_platformer::BiteAttack{};
    definition.bite.value().phase = simple_platformer::BitePhase::Recovery;
    definition.bite.value().phaseTimeRemaining = 10;
    auto first = simple_platformer::composeActor(
        definition, {}, 0, {24, 32}, simple_platformer::Patrol{{8, 32}, {40, 32}, true});
    auto second = simple_platformer::composeActor(definition, {}, 0, {40, 32});
    REQUIRE(tests::platformerMovement(first).config.maximumSpeed == 42);
    REQUIRE(simple_platformer::feetOf(first.body.bounds).x == 24);
    REQUIRE(first.brain.has_value());
    REQUIRE(first.pathFollower.has_value());
    REQUIRE(first.patrol.has_value());
    REQUIRE_FALSE(second.patrol.has_value());
    REQUIRE(tests::bite(first).phase == simple_platformer::BitePhase::Ready);
    REQUIRE(tests::bite(first).phaseTimeRemaining == 0);
    tests::health(second).current = 1;
    REQUIRE(tests::health(first).current == 4);
    REQUIRE(tests::inventory(first).slots().size() == 2);
}

TEST_CASE("Actor definitions reuse engine component validation", "[app][actors]")
{
    simple_platformer::ActorDefinition definition;
    definition.bodySize = {12.0F, 20.0F};
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
    SECTION("A tactic without senses")
    {
        definition.tactic = simple_platformer::NpcTactic::KeepDistance;
    }
    SECTION("A negative standoff")
    {
        definition.senses = simple_platformer::NpcSenses{};
        definition.tactic = simple_platformer::NpcTactic::KeepDistance;
        definition.standoffDistance = -1.0F;
    }
    SECTION("A machine without senses")
    {
        definition.machine = "test_machine";
    }
    SECTION("A machine the catalog lacks")
    {
        definition.senses = simple_platformer::NpcSenses{};
        definition.machine = "missing";
    }
    REQUIRE_THROWS_AS(
        simple_platformer::validateActorDefinition(definition, {}), std::invalid_argument);
}

TEST_CASE("Actor JSON accepts custom names and configures component choices", "[app][actors][json]")
{
    const auto animations =
        simple_platformer::loadAnimationCatalog("tests/fixtures/animations.json");
    const auto machines = simple_platformer::loadMachineCatalog("tests/fixtures/machines.json");
    const auto catalog = simple_platformer::parseActorCatalog(
        R"({
        "player":"hero", "actors":{
          "hero":{"bodySize":[12,20],"platformer":{"jumpSpeed":210},"health":5,"inventorySlots":3},
          "scout":{"flying":{"speed":25},"team":"enemy","senses":{"noticeDistance":40,"searchDuration":3},
                   "tactic":{"kind":"keepDistance","standoffDistance":30},
                   "machine":"test_machine",
                   "bodySize":[8,6],"animations":"test_actor","spriteAnchor":"center","bite":{"damage":2}}
        }})",
        "test actors",
        animations,
        machines);
    REQUIRE(catalog.player == "hero");
    auto actor = simple_platformer::composeActor(
        simple_platformer::actorDefinition(catalog, "scout"),
        animations,
        7,
        {},
        std::nullopt,
        machines);
    REQUIRE(actor.machine.has_value());
    REQUIRE(
        simple_platformer::activeNpcMachineState(
            actor.machine.value_or(simple_platformer::NpcMachine{}))
            .name == "rest");
    REQUIRE(tests::flyingMovement(actor).speed == 25);
    REQUIRE(tests::bite(actor).damage == 2);
    REQUIRE(tests::senses(actor).searchDuration == 3);
    REQUIRE(tests::brain(actor).tactic == simple_platformer::NpcTactic::KeepDistance);
    REQUIRE(tests::brain(actor).standoffDistance == 30);
    REQUIRE(tests::sprite(actor).textureId == 7);
    REQUIRE(tests::sprite(actor).anchor == simple_platformer::SpriteAnchor::BodyCenter);
    REQUIRE_FALSE(actor.platformerMovement.has_value());
    REQUIRE_THROWS_AS(
        simple_platformer::actorDefinition(catalog, "missing"), std::invalid_argument);
}

TEST_CASE(
    "Actor JSON rejects malformed and invalid definitions including unused ones",
    "[app][actors][json]")
{
    auto root = nlohmann::json::parse(
        R"({"player":"hero","actors":{"hero":{"bodySize":[12,20],"platformer":{},"health":3,"inventorySlots":2}}})");
    SECTION("Missing body size")
    {
        root["actors"]["hero"].erase("bodySize");
    }
    SECTION("Missing player reference")
    {
        root["player"] = "missing";
    }
    SECTION("Unknown animation")
    {
        root["actors"]["hero"]["animations"] = "missing";
    }
    SECTION("Unused actor references an unknown animation")
    {
        root["actors"]["unused"] = {{"flying", {{"speed", 25}}}, {"animations", "missing"}};
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
    SECTION("Unknown tactic")
    {
        root["actors"]["hero"]["senses"] = {};
        root["actors"]["hero"]["tactic"] = {{"kind", "ambusher"}};
    }
    SECTION("A standoff on a pursuer")
    {
        root["actors"]["hero"]["senses"] = {};
        root["actors"]["hero"]["tactic"] = {{"kind", "pursuer"}, {"standoffDistance", 48}};
    }
    SECTION("Unused definition")
    {
        root["actors"]["unused"] = {{"flying", {{"speed", -1}}}};
    }
    REQUIRE_THROWS_WITH(
        simple_platformer::parseActorCatalog(root.dump(), "actors.json", {}),
        Catch::Matchers::ContainsSubstring("actors.json:"));
}
