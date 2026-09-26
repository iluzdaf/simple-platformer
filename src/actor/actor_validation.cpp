#include "simple_platformer/actor/actor_validation.hpp"

#include "simple_platformer/npc/npc_state_machine.hpp"

#include <cmath>
#include <stdexcept>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/validation.hpp"

namespace simple_platformer
{
    namespace
    {
        void validateIdentity(const Actor& actor)
        {
            if (isValid(actor.id))
            {
                throw std::invalid_argument("World assigns actor IDs");
            }
        }

        void validateBody(const Actor& actor)
        {
            if (!isFinite(actor.body.bounds.position) || !isFinite(actor.body.bounds.size) ||
                !isFinite(actor.body.velocity) || actor.body.bounds.size.x <= 0.0F ||
                actor.body.bounds.size.y <= 0.0F)
            {
                throw std::invalid_argument("Actors require finite positive-sized bodies");
            }
        }

        void validateMovement(const Actor& actor)
        {
            if (actor.platformerMovement.has_value() == actor.flyingMovement.has_value())
            {
                throw std::invalid_argument("Actors require exactly one movement component");
            }
            if (actor.flyingMovement.has_value() &&
                (!std::isfinite(actor.flyingMovement->speed) || actor.flyingMovement->speed < 0.0F))
            {
                throw std::invalid_argument(
                    "Flying movement speed must be finite and non-negative");
            }
        }

        void validatePresentation(const Actor& actor)
        {
            if (actor.animator.has_value() && !actor.sprite.has_value())
            {
                throw std::invalid_argument("Animated actors require a sprite");
            }
            if (actor.animator.has_value())
            {
                requireSeconds(actor.animator->elapsed, "Actor animation elapsed");
                if (actor.animator->animationSet.clips.empty())
                {
                    throw std::invalid_argument("Actor animation data is invalid");
                }
            }
        }

        void validateCombat(const Actor& actor)
        {
            if (actor.rangedWeapon.has_value())
            {
                const RangedWeapon& weapon = *actor.rangedWeapon;
                requireSeconds(weapon.phaseTimeRemaining, "Ranged weapon phase time remaining");
                if (weapon.damage <= 0 || !isFinite(weapon.projectileSize) ||
                    weapon.projectileSize.x <= 0.0F || weapon.projectileSize.y <= 0.0F ||
                    !isFinitePositive(weapon.projectileSpeed) ||
                    !isFinitePositive(weapon.projectileLifetime) ||
                    !isFinitePositive(weapon.shootDuration) ||
                    !isFinitePositive(weapon.recoveryDuration) ||
                    !isFinite(weapon.projectileSprite.size) ||
                    weapon.projectileSprite.size.x <= 0.0F ||
                    weapon.projectileSprite.size.y <= 0.0F)
                {
                    throw std::invalid_argument("Actor ranged weapon data is invalid");
                }
            }
            if (actor.bite.has_value())
            {
                const BiteAttack& bite = *actor.bite;
                requireSeconds(bite.phaseTimeRemaining, "Bite phase time remaining");
                if (bite.damage <= 0 || !isFinite(bite.hitboxSize) || bite.hitboxSize.x <= 0.0F ||
                    bite.hitboxSize.y <= 0.0F || !std::isfinite(bite.reach) || bite.reach < 0.0F ||
                    !isFinitePositive(bite.windupDuration) ||
                    !isFinitePositive(bite.activeDuration) ||
                    !isFinitePositive(bite.recoveryDuration))
                {
                    throw std::invalid_argument("Actor bite data is invalid");
                }
            }
            if (actor.rangedWeapon.has_value() && actor.bite.has_value())
            {
                throw std::invalid_argument("An actor can have only one primary attack");
            }
            if ((actor.rangedWeapon.has_value() || actor.bite.has_value()) &&
                actor.team == Team::Neutral)
            {
                throw std::invalid_argument("Actors with attacks require a non-neutral team");
            }
            if (actor.health.has_value() &&
                (actor.health->maximum <= 0 || actor.health->current < 0 ||
                 actor.health->current > actor.health->maximum))
            {
                throw std::invalid_argument("Actor health must be within zero and its maximum");
            }
        }

        void validateNpc(const Actor& actor)
        {
            const bool hasAnyNpcComponent = actor.brain.has_value() || actor.senses.has_value() ||
                                            actor.patrol.has_value() ||
                                            actor.pathFollower.has_value();
            const bool hasRequiredNpcComponents = actor.brain.has_value() &&
                                                  actor.senses.has_value() &&
                                                  actor.pathFollower.has_value();
            if (hasAnyNpcComponent && !hasRequiredNpcComponents)
            {
                throw std::invalid_argument(
                    "NPC actors require a brain, senses, and path follower");
            }
            if (actor.machine.has_value())
            {
                if (!actor.brain.has_value())
                {
                    throw std::invalid_argument("An NPC state machine requires a brain");
                }
                validateNpcStateMachine(actor.machine->definition);
                if (actor.machine->active >= actor.machine->definition.states.size() ||
                    actor.machine->heldFor.size() != actor.machine->definition.transitions.size())
                {
                    throw std::invalid_argument("An NPC state machine must be started");
                }
                requireSeconds(actor.machine->stateElapsed, "NPC machine state elapsed");
            }
            if (actor.brain.has_value())
            {
                requireSeconds(actor.brain->stateElapsed, "NPC state elapsed");
                requireSeconds(actor.brain->targetMemoryRemaining, "NPC target memory remaining");
                if (!isFinite(actor.brain->lastSeenTargetFeet))
                {
                    throw std::invalid_argument("NPC brain runtime data is invalid");
                }
            }
            if (actor.senses.has_value())
            {
                requireSeconds(actor.senses->targetMemoryDuration, "NPC target memory duration");
                requireSeconds(actor.senses->searchDuration, "NPC search duration");
                if (!std::isfinite(actor.senses->noticeDistance) ||
                    actor.senses->noticeDistance < 0.0F ||
                    !std::isfinite(actor.senses->standoffDistance) ||
                    actor.senses->standoffDistance < 0.0F)
                {
                    throw std::invalid_argument("NPC senses data is invalid");
                }
            }
            if (actor.patrol.has_value() &&
                (!isFinite(actor.patrol->firstFeet) || !isFinite(actor.patrol->secondFeet)))
            {
                throw std::invalid_argument("NPC patrol endpoints must be finite");
            }
        }

        void validatePathFollower(const Actor& actor)
        {
            if (actor.pathFollower.has_value())
            {
                requireSeconds(actor.pathFollower->repathRemaining, "NPC repath time remaining");
                requireSeconds(actor.pathFollower->programElapsed, "NPC path program elapsed");
                if (!isFinitePositive(actor.pathFollower->repathCooldown))
                {
                    throw std::invalid_argument("NPC path timing is invalid");
                }
            }
        }
    }

    void validateActor(const Actor& actor)
    {
        validateIdentity(actor);
        validateBody(actor);
        validateMovement(actor);
        validatePresentation(actor);
        validateCombat(actor);
        validateNpc(actor);
        validatePathFollower(actor);
    }
}
