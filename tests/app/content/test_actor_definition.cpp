#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "content/actor_catalog.hpp"
#include "content/actor_definition.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "support/actor_components.hpp"

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
    definition.ranged->lastFiredTimeSeconds = 3.0;
    auto actor = simple_platformer::composeActor(definition, {}, 9);
    REQUIRE(tests::rangedWeapon(actor).damage == 2);
    REQUIRE(tests::rangedWeapon(actor).projectileSpeed == 120);
    REQUIRE(tests::rangedWeapon(actor).projectileSprite.textureId == 9);
    REQUIRE(tests::rangedWeapon(actor).projectileSprite.region.position.x == 4);
    REQUIRE(tests::rangedWeapon(actor).phase == simple_platformer::RangedPhase::Ready);
    REQUIRE_FALSE(tests::rangedWeapon(actor).lastFiredTimeSeconds.has_value());
}

TEST_CASE("Contact damage definitions compose fresh independent state", "[app][actors][contact]")
{
    const auto catalog = simple_platformer::parseActorCatalog(
        R"({"player":"runner","actors":{"runner":{"bodySize":[12,12],"team":"player",
             "health":3,"inventorySlots":1,"platformer":{},"contactDamage":{"damage":2}}}})",
        "contact damage",
        {});
    auto definition = simple_platformer::actorDefinition(catalog, "runner");
    if (!definition.contactDamage.has_value())
    {
        throw std::logic_error("Contact damage was not parsed");
    }
    auto& contactDamage = *definition.contactDamage;
    contactDamage.active = true;
    contactDamage.actorsHit.push_back(simple_platformer::ActorId{7});

    auto composed = simple_platformer::composeActor(definition, {}, 0);
    REQUIRE(tests::contactDamage(composed).damage == 2);
    REQUIRE_FALSE(tests::contactDamage(composed).active);
    REQUIRE(tests::contactDamage(composed).actorsHit.empty());
}

TEST_CASE(
    "Contact damage coexists with primary attacks and either movement",
    "[app][actors][contact]")
{
    simple_platformer::ActorDefinition definition;
    definition.bodySize = {12.0F, 12.0F};
    definition.team = simple_platformer::Team::Enemy;
    definition.contactDamage = simple_platformer::ContactDamage{};
    SECTION("Walking with a bite")
    {
        definition.platformer = simple_platformer::PlatformerMovementConfig{};
        definition.bite = simple_platformer::BiteAttack{};
    }
    SECTION("Flying with a ranged weapon")
    {
        definition.flying = simple_platformer::FlyingMovement{};
        definition.ranged = simple_platformer::RangedWeapon{};
    }
    REQUIRE_NOTHROW(simple_platformer::validateActorDefinition(definition, {}));
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
    REQUIRE_FALSE(tests::perception(first).targetVisible);
    REQUIRE_FALSE(tests::perception(first).heardLanding);
    tests::perception(first).targetVisible = true;
    tests::perception(first).heardLanding = true;
    REQUIRE_FALSE(tests::perception(second).targetVisible);
    REQUIRE_FALSE(tests::perception(second).heardLanding);
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
    SECTION("Invalid contact damage")
    {
        definition.team = simple_platformer::Team::Enemy;
        definition.contactDamage = simple_platformer::ContactDamage{};
        definition.contactDamage->damage = 0;
    }
    SECTION("Neutral contact damage")
    {
        definition.contactDamage = simple_platformer::ContactDamage{};
    }
    SECTION("A tactic without senses")
    {
        definition.tactic = simple_platformer::NpcTactic::KeepDistance;
    }
    SECTION("A negative standoff")
    {
        definition.senses = simple_platformer::NpcSenses{};
        definition.senses->standoffDistance = -1.0F;
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
