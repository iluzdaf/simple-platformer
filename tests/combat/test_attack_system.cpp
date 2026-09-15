#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

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

namespace
{
    simple_platformer::Actor makeActor(
        glm::vec2 position,
        simple_platformer::Team team,
        int health = 3)
    {
        simple_platformer::Actor actor;
        actor.body.bounds = {position, {12.0F, 12.0F}};
        actor.platformerMovement = simple_platformer::PlatformerMovement{};
        actor.health = simple_platformer::Health{health, health};
        actor.team = team;
        return actor;
    }

    int healthOf(const simple_platformer::World& world, simple_platformer::ActorId id)
    {
        const simple_platformer::Actor* actor = world.findActor(id);
        if (actor == nullptr || !actor->health.has_value())
        {
            throw std::logic_error("Test actor has no health");
        }
        return actor->health->current;
    }

    simple_platformer::BitePhase bitePhaseOf(
        const simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        const simple_platformer::Actor* actor = world.findActor(id);
        if (actor == nullptr || !actor->bite.has_value())
        {
            throw std::logic_error("Test actor has no bite");
        }
        return actor->bite->phase;
    }
}

TEST_CASE("A ranged weapon queues a projectile in the actor's facing direction", "[combat][weapon]")
{
    simple_platformer::World world;
    simple_platformer::Actor actor = makeActor({20.0F, 20.0F}, simple_platformer::Team::Player);
    actor.facing = simple_platformer::Facing::Left;
    actor.rangedWeapon = simple_platformer::RangedWeapon{};
    actor.rangedWeapon->projectileSprite.size = {8.0F, 6.0F};
    actor.intentions.primaryAttackPressed = true;
    const simple_platformer::ActorId shooter = world.addActor(actor);
    simple_platformer::WorldRequests requests;

    simple_platformer::updateAttacks(world, requests, 0.1F);

    REQUIRE(world.projectiles().empty());
    simple_platformer::updateLifeState(world, requests, 0.0F);
    REQUIRE(world.projectiles().size() == 1);
    const simple_platformer::Projectile& projectile = world.projectiles().front();
    REQUIRE(projectile.owner == shooter);
    REQUIRE(projectile.team == simple_platformer::Team::Player);
    REQUIRE(projectile.velocity.x < 0.0F);
    REQUIRE(projectile.bounds.position.x == 16.0F);
    REQUIRE(projectile.bounds.size.x == 4.0F);
    REQUIRE(projectile.sprite.size.x == 8.0F);
}

TEST_CASE("A ranged weapon observes its cooldown", "[combat][weapon]")
{
    simple_platformer::World world;
    simple_platformer::Actor actor = makeActor({20.0F, 20.0F}, simple_platformer::Team::Player);
    actor.rangedWeapon = simple_platformer::RangedWeapon{};
    actor.intentions.primaryAttackPressed = true;
    const simple_platformer::ActorId shooter = world.addActor(actor);
    simple_platformer::WorldRequests requests;

    simple_platformer::updateAttacks(world, requests, 0.0F);
    simple_platformer::updateLifeState(world, requests, 0.0F);
    simple_platformer::updateAttacks(world, requests, 0.1F);
    simple_platformer::updateLifeState(world, requests, 0.0F);
    REQUIRE(world.projectiles().size() == 1);
    REQUIRE(world.projectiles().front().velocity.x > 0.0F);

    simple_platformer::Actor* stored = world.findActor(shooter);
    REQUIRE(stored != nullptr);
    stored->intentions.primaryAttackPressed = true;
    simple_platformer::updateAttacks(world, requests, 0.15F);
    simple_platformer::updateLifeState(world, requests, 0.0F);
    REQUIRE(world.projectiles().size() == 2);
}

TEST_CASE("A bite uses windup active and recovery phases", "[combat][bite]")
{
    simple_platformer::World world;
    simple_platformer::Actor attacker = makeActor({10.0F, 10.0F}, simple_platformer::Team::Enemy);
    attacker.bite = simple_platformer::BiteAttack{};
    attacker.bite->reach = 0.0F;
    attacker.intentions.primaryAttackPressed = true;
    const simple_platformer::ActorId attackerId = world.addActor(attacker);
    const simple_platformer::ActorId target =
        world.addActor(makeActor({22.0F, 10.0F}, simple_platformer::Team::Player));
    simple_platformer::WorldRequests requests;

    simple_platformer::updateAttacks(world, requests, 0.1F);
    REQUIRE(healthOf(world, target) == 3);
    REQUIRE(bitePhaseOf(world, attackerId) == simple_platformer::BitePhase::Windup);

    simple_platformer::updateAttacks(world, requests, 0.12F);
    simple_platformer::updateLifeState(world, requests, 0.0F);
    REQUIRE(healthOf(world, target) == 2);
    REQUIRE(bitePhaseOf(world, attackerId) == simple_platformer::BitePhase::Active);

    simple_platformer::updateAttacks(world, requests, 0.04F);
    simple_platformer::updateLifeState(world, requests, 0.0F);
    REQUIRE(healthOf(world, target) == 2);

    simple_platformer::updateAttacks(world, requests, 0.04F);
    REQUIRE(bitePhaseOf(world, attackerId) == simple_platformer::BitePhase::Recovery);
    simple_platformer::updateAttacks(world, requests, 0.30F);
    REQUIRE(bitePhaseOf(world, attackerId) == simple_platformer::BitePhase::Ready);
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
    simple_platformer::updateLifeState(world, requests, 0.0F);

    REQUIRE(healthOf(world, target) == 3);
    REQUIRE(world.findActor(attackerId)->body.bounds.position.x == 10.0F);
}

TEST_CASE("A committed bite completes but can miss", "[combat][bite]")
{
    simple_platformer::World world;
    simple_platformer::Actor attacker = makeActor({10.0F, 10.0F}, simple_platformer::Team::Enemy);
    attacker.bite = simple_platformer::BiteAttack{};
    attacker.bite->reach = 0.0F;
    attacker.intentions.primaryAttackPressed = true;
    const simple_platformer::ActorId attackerId = world.addActor(attacker);
    const simple_platformer::ActorId target =
        world.addActor(makeActor({22.0F, 10.0F}, simple_platformer::Team::Player));
    simple_platformer::WorldRequests requests;

    simple_platformer::updateAttacks(world, requests, 0.0F);
    simple_platformer::Actor* movedTarget = world.findActor(target);
    REQUIRE(movedTarget != nullptr);
    movedTarget->body.bounds.position.x = 100.0F;
    simple_platformer::updateAttacks(world, requests, 0.51F);
    simple_platformer::updateLifeState(world, requests, 0.0F);

    REQUIRE(healthOf(world, target) == 3);
    REQUIRE(bitePhaseOf(world, attackerId) == simple_platformer::BitePhase::Ready);
}

TEST_CASE("Dying actors cannot begin attacks", "[combat][lifecycle]")
{
    simple_platformer::World world;
    simple_platformer::Actor actor = makeActor({20.0F, 20.0F}, simple_platformer::Team::Player);
    actor.rangedWeapon = simple_platformer::RangedWeapon{};
    actor.intentions.primaryAttackPressed = true;
    actor.life = simple_platformer::LifeState::Dying;
    world.addActor(actor);
    simple_platformer::WorldRequests requests;

    simple_platformer::updateAttacks(world, requests, 0.1F);
    simple_platformer::updateLifeState(world, requests, 0.0F);

    REQUIRE(world.projectiles().empty());
}

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

    REQUIRE(right.position.x == 36.0F);
    REQUIRE(left.position.x == 6.0F);
    REQUIRE(right.position.y == 32.0F);
    REQUIRE(left.position.y == 32.0F);
}
