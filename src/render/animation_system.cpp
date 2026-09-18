#include "simple_platformer/render/animation_system.hpp"

#include <cmath>
#include <stdexcept>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    namespace
    {
        void updateActorAnimations(World& world, float deltaTime)
        {
            for (Actor& actor : world.actors())
            {
                if (!actor.animator.has_value())
                {
                    continue;
                }
                if ((!actor.platformerMovement.has_value() && !actor.flyingMovement.has_value()) ||
                    !actor.sprite.has_value())
                {
                    throw std::logic_error("An animated actor is missing a required component");
                }

                const bool biting = actor.bite.has_value() && actor.bite->phase != BitePhase::Ready;
                const bool shooting = actor.rangedWeapon.has_value() &&
                                      actor.rangedWeapon->phase == RangedPhase::Shoot;
                const bool attacking = biting || shooting;
                const bool grounded = actor.platformerMovement.has_value()
                                          ? actor.platformerMovement->grounded
                                          : true;
                const AnimationName selected = selectActorAnimation(
                    actor.life == LifeState::Dying, attacking, grounded, actor.body.velocity);
                updateAnimation(*actor.animator, *actor.sprite, selected, deltaTime);
            }
        }
    }

    void updateWorldAnimations(World& world, float deltaTime)
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0F)
        {
            throw std::invalid_argument("Animation delta time must be finite and non-negative");
        }
        updateActorAnimations(world, deltaTime);
    }
}
