#include <catch2/catch_test_macros.hpp>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_lifecycle.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/require_near.hpp"

namespace
{
    simple_platformer::Actor makeActor(glm::vec2 topLeft, simple_platformer::Team team)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F})
            .at(topLeft)
            .platforming()
            .withHealth(3, 3)
            .onTeam(team);
    }
}

// Attack phases and damage

TEST_CASE("A bite passes through windup, active and recovery phases", "[combat][bite]")
{
    simple_platformer::World world;
    simple_platformer::Actor attacker = makeActor({10.0F, 10.0F}, simple_platformer::Team::Enemy);
    attacker.bite = simple_platformer::BiteAttack{};
    tests::bite(attacker).reach = 0.0F;
    attacker.intentions.primaryAttackPressed = true;
    const simple_platformer::ActorId attackerId = world.addActor(attacker);
    const simple_platformer::ActorId target =
        world.addActor(makeActor({22.0F, 10.0F}, simple_platformer::Team::Player));
    simple_platformer::WorldRequests requests;

    simple_platformer::updateAttacks(world, requests, 0.1F);
    REQUIRE(tests::health(world, target).current == 3);
    REQUIRE(tests::bite(world, attackerId).phase == simple_platformer::BitePhase::Windup);

    simple_platformer::updateAttacks(world, requests, 0.12F);
    simple_platformer::updateActorLifecycle(world, requests, 0.0F);
    REQUIRE(tests::health(world, target).current == 2);
    REQUIRE(tests::bite(world, attackerId).phase == simple_platformer::BitePhase::Active);

    simple_platformer::updateAttacks(world, requests, 0.04F);
    simple_platformer::updateActorLifecycle(world, requests, 0.0F);
    REQUIRE(tests::health(world, target).current == 2);

    simple_platformer::updateAttacks(world, requests, 0.04F);
    REQUIRE(tests::bite(world, attackerId).phase == simple_platformer::BitePhase::Recovery);
    simple_platformer::updateAttacks(world, requests, 0.30F);
    REQUIRE(tests::bite(world, attackerId).phase == simple_platformer::BitePhase::Ready);
}

TEST_CASE("A committed bite completes but can miss", "[combat][bite]")
{
    simple_platformer::World world;
    simple_platformer::Actor attacker = makeActor({10.0F, 10.0F}, simple_platformer::Team::Enemy);
    attacker.bite = simple_platformer::BiteAttack{};
    tests::bite(attacker).reach = 0.0F;
    attacker.intentions.primaryAttackPressed = true;
    const simple_platformer::ActorId attackerId = world.addActor(attacker);
    const simple_platformer::ActorId target =
        world.addActor(makeActor({22.0F, 10.0F}, simple_platformer::Team::Player));
    simple_platformer::WorldRequests requests;

    simple_platformer::updateAttacks(world, requests, 0.0F);
    simple_platformer::Actor& movedTarget = tests::actor(world, target);
    movedTarget.body.bounds.topLeft.x = 100.0F;
    simple_platformer::updateAttacks(world, requests, 0.51F);
    simple_platformer::updateActorLifecycle(world, requests, 0.0F);

    REQUIRE(tests::health(world, target).current == 3);
    REQUIRE(tests::bite(world, attackerId).phase == simple_platformer::BitePhase::Ready);
}

TEST_CASE("A ready bite is harmless and never lunges", "[combat][bite]")
{
    simple_platformer::World world;
    simple_platformer::Actor attacker = makeActor({10.0F, 10.0F}, simple_platformer::Team::Enemy);
    attacker.bite = simple_platformer::BiteAttack{};
    const simple_platformer::ActorId attackerId = world.addActor(attacker);
    const simple_platformer::ActorId target =
        world.addActor(makeActor({15.0F, 10.0F}, simple_platformer::Team::Player));
    simple_platformer::WorldRequests requests;

    simple_platformer::updateAttacks(world, requests, 1.0F);
    simple_platformer::updateActorLifecycle(world, requests, 0.0F);

    REQUIRE(tests::health(world, target).current == 3);
    REQUIRE(tests::actor(world, attackerId).body.bounds.topLeft.x == 10.0F);
}

// Hitbox placement

TEST_CASE("Bite hitboxes are placed in the retained facing direction", "[combat][bite]")
{
    const simple_platformer::Aabb actor{{20.0F, 30.0F}, {12.0F, 12.0F}};
    simple_platformer::BiteAttack bite;
    bite.hitboxSize = {10.0F, 8.0F};
    bite.reach = 4.0F;

    const simple_platformer::Aabb right =
        simple_platformer::biteHitbox(actor, bite, simple_platformer::Facing::Right);
    const simple_platformer::Aabb left =
        simple_platformer::biteHitbox(actor, bite, simple_platformer::Facing::Left);

    REQUIRE(right.topLeft.x == 36.0F);
    REQUIRE(left.topLeft.x == 6.0F);
    REQUIRE(right.topLeft.y == 32.0F);
    REQUIRE(left.topLeft.y == 32.0F);
}

// Updates crossing phase boundaries

TEST_CASE(
    "A new bite keeps its windup time and later hits even if an update skips the active phase",
    "[combat][bite]")
{
    simple_platformer::World world;
    simple_platformer::Actor attacker = makeActor({10.0F, 10.0F}, simple_platformer::Team::Enemy);
    attacker.bite = simple_platformer::BiteAttack{};
    tests::bite(attacker).reach = 0.0F;
    attacker.intentions.primaryAttackPressed = true;
    const auto attackerId = world.addActor(attacker);
    const auto targetId =
        world.addActor(makeActor({22.0F, 10.0F}, simple_platformer::Team::Player));
    simple_platformer::WorldRequests requests;

    // Starting an attack does not spend this update's time on its new Windup.
    simple_platformer::updateAttacks(world, requests, 1.0F);
    simple_platformer::updateActorLifecycle(world, requests, 0.0F);
    REQUIRE(tests::bite(world, attackerId).phase == simple_platformer::BitePhase::Windup);
    REQUIRE_NEAR(tests::bite(world, attackerId).phaseTimeRemaining, 0.12F);
    REQUIRE(tests::health(world, targetId).current == 3);

    SECTION("The update crosses Active and ends in Recovery")
    {
        simple_platformer::updateAttacks(world, requests, 0.25F);
        REQUIRE(tests::bite(world, attackerId).phase == simple_platformer::BitePhase::Recovery);
        REQUIRE_NEAR(tests::bite(world, attackerId).phaseTimeRemaining, 0.25F);
    }
    SECTION("The update crosses all phases and ends Ready")
    {
        simple_platformer::updateAttacks(world, requests, 0.51F);
        REQUIRE(tests::bite(world, attackerId).phase == simple_platformer::BitePhase::Ready);
        REQUIRE(tests::bite(world, attackerId).phaseTimeRemaining == 0.0F);
    }

    // Both updates must check hits, even though neither ends in Active.
    REQUIRE(tests::bite(world, attackerId).actorsHit.size() == 1);
    REQUIRE(tests::bite(world, attackerId).actorsHit.front() == targetId);
    simple_platformer::updateActorLifecycle(world, requests, 0.0F);
    REQUIRE(tests::health(world, targetId).current == 2);
    tests::actor(world, attackerId).intentions.primaryAttackPressed = false;
    simple_platformer::updateAttacks(world, requests, 0.0F);
    simple_platformer::updateActorLifecycle(world, requests, 0.0F);
    REQUIRE(tests::health(world, targetId).current == 2);
}
