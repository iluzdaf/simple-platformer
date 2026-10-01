#include <catch2/catch_test_macros.hpp>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/actor/lifecycle.hpp"
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

TEST_CASE("A ranged weapon queues a projectile in its aim direction", "[combat][weapon]")
{
    simple_platformer::World world;
    simple_platformer::Actor actor = makeActor({20.0F, 20.0F}, simple_platformer::Team::Player);
    actor.facing = simple_platformer::Facing::Right;
    actor.rangedWeapon = simple_platformer::RangedWeapon{};
    tests::rangedWeapon(actor).projectileSprite.size = {8.0F, 6.0F};
    actor.intentions.aimDirection = {-1.0F, 0.0F};
    actor.intentions.primaryAttackPressed = true;
    const simple_platformer::ActorId shooter = world.addActor(actor);
    simple_platformer::WorldRequests requests;
    world.advanceSimulationTime(0.25F);

    simple_platformer::updateAttacks(world, requests, 0.1F);

    REQUIRE(world.projectiles().empty());
    REQUIRE_NEAR(tests::rangedWeapon(world, shooter).lastFiredTimeSeconds.value_or(-1.0), 0.25);
    const auto noises = world.takeNoises();
    REQUIRE(noises.size() == 1);
    REQUIRE(noises.front().source == shooter);
    REQUIRE(noises.front().kind == simple_platformer::NoiseKind::Shot);
    REQUIRE(
        noises.front().feet == simple_platformer::feetOf(tests::actor(world, shooter).body.bounds));
    REQUIRE(tests::rangedWeapon(world, shooter).phase == simple_platformer::RangedPhase::Shoot);
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(world.projectiles().size() == 1);
    const simple_platformer::Projectile& projectile = world.projectiles().front();
    REQUIRE(projectile.owner == shooter);
    REQUIRE(projectile.team == simple_platformer::Team::Player);
    REQUIRE(projectile.velocity.x < 0.0F);
    REQUIRE(projectile.bounds.topLeft.x == 16.0F);
    REQUIRE(projectile.bounds.size.x == 4.0F);
    REQUIRE(projectile.sprite.size.x == 8.0F);
}

TEST_CASE("A ranged weapon normalises a diagonal aim direction", "[combat][weapon]")
{
    simple_platformer::World world;
    simple_platformer::Actor actor = makeActor({20.0F, 20.0F}, simple_platformer::Team::Player);
    actor.rangedWeapon = simple_platformer::RangedWeapon{};
    actor.intentions.aimDirection = {3.0F, 4.0F};
    actor.intentions.primaryAttackPressed = true;
    world.addActor(actor);
    simple_platformer::WorldRequests requests;

    simple_platformer::updateAttacks(world, requests, 0.0F);
    simple_platformer::applyWorldRequests(world, requests);

    REQUIRE(world.projectiles().size() == 1);
    REQUIRE_NEAR(world.projectiles().front().velocity.x, 108.0F);
    REQUIRE_NEAR(world.projectiles().front().velocity.y, 144.0F);
}

TEST_CASE("A ranged weapon does not fire without an aim direction", "[combat][weapon]")
{
    simple_platformer::World world;
    simple_platformer::Actor actor = makeActor({20.0F, 20.0F}, simple_platformer::Team::Player);
    actor.rangedWeapon = simple_platformer::RangedWeapon{};
    actor.intentions.primaryAttackPressed = true;
    const simple_platformer::ActorId shooter = world.addActor(actor);
    simple_platformer::WorldRequests requests;

    simple_platformer::updateAttacks(world, requests, 0.0F);
    simple_platformer::applyWorldRequests(world, requests);

    REQUIRE(world.projectiles().empty());
    REQUIRE(world.takeNoises().empty());
    REQUIRE_FALSE(tests::rangedWeapon(world, shooter).lastFiredTimeSeconds.has_value());
    REQUIRE(tests::rangedWeapon(world, shooter).phase == simple_platformer::RangedPhase::Ready);
}

TEST_CASE("A ranged weapon uses shoot and recovery phases", "[combat][weapon]")
{
    simple_platformer::World world;
    simple_platformer::Actor actor = makeActor({20.0F, 20.0F}, simple_platformer::Team::Player);
    actor.rangedWeapon = simple_platformer::RangedWeapon{};
    actor.intentions.aimDirection = {1.0F, 0.0F};
    actor.intentions.primaryAttackPressed = true;
    const simple_platformer::ActorId shooter = world.addActor(actor);
    simple_platformer::WorldRequests requests;

    simple_platformer::updateAttacks(world, requests, 0.0F);
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(tests::rangedWeapon(world, shooter).phase == simple_platformer::RangedPhase::Shoot);

    world.advanceSimulationTime(0.15F);
    simple_platformer::updateAttacks(world, requests, 0.15F);
    REQUIRE_NEAR(tests::rangedWeapon(world, shooter).lastFiredTimeSeconds.value_or(-1.0), 0.0);
    REQUIRE(world.takeNoises().size() == 1);
    REQUIRE(tests::rangedWeapon(world, shooter).phase == simple_platformer::RangedPhase::Recovery);
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(world.projectiles().size() == 1);
    REQUIRE(world.projectiles().front().velocity.x > 0.0F);

    world.advanceSimulationTime(0.20F);
    simple_platformer::updateAttacks(world, requests, 0.20F);
    REQUIRE_NEAR(tests::rangedWeapon(world, shooter).lastFiredTimeSeconds.value_or(-1.0), 0.0);
    REQUIRE(world.takeNoises().empty());
    REQUIRE(tests::rangedWeapon(world, shooter).phase == simple_platformer::RangedPhase::Ready);

    simple_platformer::Actor& stored = tests::actor(world, shooter);
    stored.intentions.primaryAttackPressed = true;
    simple_platformer::updateAttacks(world, requests, 0.0F);
    REQUIRE_NEAR(tests::rangedWeapon(world, shooter).lastFiredTimeSeconds.value_or(-1.0), 0.35);
    REQUIRE(world.takeNoises().size() == 1);
    REQUIRE(tests::rangedWeapon(world, shooter).phase == simple_platformer::RangedPhase::Shoot);
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(world.projectiles().size() == 2);
}

TEST_CASE("Dying actors cannot begin ranged attacks", "[combat][weapon][lifecycle]")
{
    simple_platformer::World world;
    simple_platformer::Actor actor = makeActor({20.0F, 20.0F}, simple_platformer::Team::Player);
    actor.rangedWeapon = simple_platformer::RangedWeapon{};
    tests::rangedWeapon(actor).phase = simple_platformer::RangedPhase::Shoot;
    tests::rangedWeapon(actor).phaseTimeRemaining = tests::rangedWeapon(actor).shootDuration;
    actor.intentions.primaryAttackPressed = true;
    actor.life = simple_platformer::LifeState::Dying;
    const simple_platformer::ActorId actorId = world.addActor(actor);
    simple_platformer::WorldRequests requests;

    simple_platformer::updateAttacks(world, requests, 0.1F);
    REQUIRE(tests::rangedWeapon(world, actorId).phase == simple_platformer::RangedPhase::Ready);
    simple_platformer::applyWorldRequests(world, requests);

    REQUIRE(world.projectiles().empty());
}
