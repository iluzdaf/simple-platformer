#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/animation_system.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"

namespace
{
    simple_platformer::AnimationClip clip(simple_platformer::AnimationName name, float left)
    {
        return {name, {{{left, 0.0F}, {1.0F, 1.0F}}}, 0.1F, true};
    }

    simple_platformer::Actor makeAnimatedActor()
    {
        using simple_platformer::AnimationName;

        simple_platformer::Animator animator;
        animator.animationSet = {{
            clip(AnimationName::Idle, 0.0F),
            clip(AnimationName::Move, 1.0F),
            clip(AnimationName::Jump, 2.0F),
            clip(AnimationName::Fall, 3.0F),
            clip(AnimationName::Attack, 4.0F),
            clip(AnimationName::Death, 5.0F),
        }};
        simple_platformer::Actor actor = tests::ActorBuilder::sized({12.0F, 12.0F})
                                             .at({0.0F, 0.0F})
                                             .walking()
                                             .withSprite({0, {}, {1.0F, 1.0F}})
                                             .withAnimator(animator);
        tests::platformerMovement(actor).grounded = true;
        return actor;
    }
}

TEST_CASE("A ranged actor uses Attack only during its Shoot phase", "[render][animation][system]")
{
    simple_platformer::World world;
    simple_platformer::Actor rangedActor = makeAnimatedActor();
    rangedActor.team = simple_platformer::Team::Player;
    rangedActor.rangedWeapon = simple_platformer::RangedWeapon{};
    tests::rangedWeapon(rangedActor).phase = simple_platformer::RangedPhase::Shoot;
    tests::rangedWeapon(rangedActor).phaseTimeRemaining =
        tests::rangedWeapon(rangedActor).shootDuration;
    const simple_platformer::ActorId id = world.addActor(rangedActor);

    simple_platformer::updateWorldAnimations(world, 0.0F);
    REQUIRE(tests::animator(world, id).current == simple_platformer::AnimationName::Attack);

    tests::rangedWeapon(world, id).phase = simple_platformer::RangedPhase::Recovery;
    tests::rangedWeapon(world, id).phaseTimeRemaining =
        tests::rangedWeapon(world, id).recoveryDuration;
    tests::actor(world, id).body.velocity.x = 10.0F;
    simple_platformer::updateWorldAnimations(world, 0.0F);
    REQUIRE(tests::animator(world, id).current == simple_platformer::AnimationName::Move);
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
        tests::bite(world, id).phase = phase;
        simple_platformer::updateWorldAnimations(world, 0.0F);
        REQUIRE(tests::animator(world, id).current == simple_platformer::AnimationName::Attack);
    }
}

TEST_CASE("Death animation has priority over a shot", "[render][animation][system]")
{
    simple_platformer::World world;
    simple_platformer::Actor dyingActor = makeAnimatedActor();
    dyingActor.team = simple_platformer::Team::Player;
    dyingActor.life = simple_platformer::LifeState::Dying;
    dyingActor.rangedWeapon = simple_platformer::RangedWeapon{};
    tests::rangedWeapon(dyingActor).phase = simple_platformer::RangedPhase::Shoot;
    tests::rangedWeapon(dyingActor).phaseTimeRemaining =
        tests::rangedWeapon(dyingActor).shootDuration;
    const simple_platformer::ActorId id = world.addActor(dyingActor);

    simple_platformer::updateWorldAnimations(world, 0.0F);

    REQUIRE(tests::animator(world, id).current == simple_platformer::AnimationName::Death);
}

TEST_CASE("World animation rejects a negative delta time", "[render][animation][system]")
{
    simple_platformer::World world;

    REQUIRE_THROWS_AS(
        simple_platformer::updateWorldAnimations(world, -0.1F), std::invalid_argument);
}
