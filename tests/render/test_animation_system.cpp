#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/animation_system.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/world.hpp"

namespace
{
    simple_platformer::AnimationClip clip(simple_platformer::AnimationName name, float left)
    {
        return {name, {{{left, 0.0F}, {1.0F, 1.0F}}}, 0.1F, true};
    }

    simple_platformer::Actor makeAnimatedActor()
    {
        using simple_platformer::AnimationName;

        simple_platformer::Actor actor;
        actor.body.bounds = {{0.0F, 0.0F}, {12.0F, 12.0F}};
        actor.platformerMovement = simple_platformer::PlatformerMovement{};
        actor.platformerMovement->grounded = true;
        actor.sprite = simple_platformer::Sprite{0, {}, {1.0F, 1.0F}};
        actor.animator = simple_platformer::Animator{};
        actor.animator->animationSet = {{
            clip(AnimationName::Idle, 0.0F),
            clip(AnimationName::Move, 1.0F),
            clip(AnimationName::Jump, 2.0F),
            clip(AnimationName::Fall, 3.0F),
            clip(AnimationName::Attack, 4.0F),
            clip(AnimationName::Death, 5.0F),
        }};
        return actor;
    }

    simple_platformer::Actor& actor(simple_platformer::World& world, simple_platformer::ActorId id)
    {
        simple_platformer::Actor* found = world.findActor(id);
        if (found == nullptr)
        {
            throw std::logic_error("Test actor was not found");
        }
        return *found;
    }

    simple_platformer::Animator& animator(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        simple_platformer::Actor& found = actor(world, id);
        if (!found.animator.has_value())
        {
            throw std::logic_error("Test actor has no animator");
        }
        return *found.animator;
    }

    simple_platformer::RangedWeapon& rangedWeapon(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        simple_platformer::Actor& found = actor(world, id);
        if (!found.rangedWeapon.has_value())
        {
            throw std::logic_error("Test actor has no ranged weapon");
        }
        return *found.rangedWeapon;
    }

    simple_platformer::BiteAttack& bite(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        simple_platformer::Actor& found = actor(world, id);
        if (!found.bite.has_value())
        {
            throw std::logic_error("Test actor has no bite attack");
        }
        return *found.bite;
    }
}

TEST_CASE("A ranged actor uses Attack only during its Shoot phase", "[render][animation][system]")
{
    simple_platformer::World world;
    simple_platformer::Actor rangedActor = makeAnimatedActor();
    rangedActor.team = simple_platformer::Team::Player;
    rangedActor.rangedWeapon = simple_platformer::RangedWeapon{};
    rangedActor.rangedWeapon->phase = simple_platformer::RangedPhase::Shoot;
    rangedActor.rangedWeapon->phaseTimeRemaining = rangedActor.rangedWeapon->shootDuration;
    const simple_platformer::ActorId id = world.addActor(rangedActor);

    simple_platformer::updateWorldAnimations(world, 0.0F);
    REQUIRE(animator(world, id).current == simple_platformer::AnimationName::Attack);

    rangedWeapon(world, id).phase = simple_platformer::RangedPhase::Recovery;
    rangedWeapon(world, id).phaseTimeRemaining = rangedWeapon(world, id).recoveryDuration;
    actor(world, id).body.velocity.x = 10.0F;
    simple_platformer::updateWorldAnimations(world, 0.0F);
    REQUIRE(animator(world, id).current == simple_platformer::AnimationName::Move);
}

TEST_CASE("Every committed bite phase uses Attack", "[render][animation][system]")
{
    simple_platformer::World world;
    simple_platformer::Actor bitingActor = makeAnimatedActor();
    bitingActor.team = simple_platformer::Team::Enemy;
    bitingActor.bite = simple_platformer::BiteAttack{};
    const simple_platformer::ActorId id = world.addActor(bitingActor);

    for (const simple_platformer::BitePhase phase : {
             simple_platformer::BitePhase::Windup,
             simple_platformer::BitePhase::Active,
             simple_platformer::BitePhase::Recovery,
         })
    {
        bite(world, id).phase = phase;
        simple_platformer::updateWorldAnimations(world, 0.0F);
        REQUIRE(animator(world, id).current == simple_platformer::AnimationName::Attack);
    }
}

TEST_CASE("Death animation has priority over a shot", "[render][animation][system]")
{
    simple_platformer::World world;
    simple_platformer::Actor dyingActor = makeAnimatedActor();
    dyingActor.team = simple_platformer::Team::Player;
    dyingActor.life = simple_platformer::LifeState::Dying;
    dyingActor.rangedWeapon = simple_platformer::RangedWeapon{};
    dyingActor.rangedWeapon->phase = simple_platformer::RangedPhase::Shoot;
    dyingActor.rangedWeapon->phaseTimeRemaining = dyingActor.rangedWeapon->shootDuration;
    const simple_platformer::ActorId id = world.addActor(dyingActor);

    simple_platformer::updateWorldAnimations(world, 0.0F);

    REQUIRE(animator(world, id).current == simple_platformer::AnimationName::Death);
}

TEST_CASE("World animation rejects a negative delta time", "[render][animation][system]")
{
    simple_platformer::World world;

    REQUIRE_THROWS_AS(
        simple_platformer::updateWorldAnimations(world, -0.1F), std::invalid_argument);
}
