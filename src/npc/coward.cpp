#include "simple_platformer/npc/coward.hpp"

#include <optional>
#include <stdexcept>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_facts.hpp"
#include "simple_platformer/npc/npc_navigation.hpp"
#include "simple_platformer/npc/npc_behaviour.hpp"
#include "simple_platformer/npc/npc_update.hpp"

namespace simple_platformer
{
    namespace
    {
        constexpr float RecoverySeconds = 1.5F;

        NpcState patrolOrIdle(const NpcFacts& facts)
        {
            return facts.hasPatrol ? NpcState::Patrol : NpcState::Idle;
        }

        void updateFleeState(
            const NpcUpdate& update,
            Actor& actor,
            const NpcBrain& brain,
            PathFollower& follower,
            const Actor* target)
        {
            if (target == nullptr || !actor.patrol.has_value())
            {
                clearPath(follower);
                return;
            }
            const Patrol& patrol = *actor.patrol;
            const glm::vec2 left =
                patrol.firstFeet.x < patrol.secondFeet.x ? patrol.firstFeet : patrol.secondFeet;
            const glm::vec2 right =
                patrol.firstFeet.x < patrol.secondFeet.x ? patrol.secondFeet : patrol.firstFeet;
            const glm::vec2 feet = feetOf(actor.body.bounds);
            const glm::vec2 refuge = feet.x < brain.lastKnownTargetFeet.x ? left : right;
            if (glm::distance(feet, refuge) <= 1.0F)
            {
                clearPath(follower);
                aimToward(actor, brain.lastKnownTargetFeet);
                return;
            }
            actor.intentions = intentionsToReach(update, actor, follower, refuge);
        }

        glm::vec2 patrolGoal(const Patrol& patrol)
        {
            return patrol.headingToSecond ? patrol.secondFeet : patrol.firstFeet;
        }

        void updatePatrolState(const NpcUpdate& update, Actor& actor, PathFollower& follower)
        {
            if (!actor.patrol.has_value())
            {
                throw std::logic_error("A patrolling NPC is missing its patrol");
            }
            Patrol& patrol = *actor.patrol;
            actor.intentions = intentionsToReach(update, actor, follower, patrolGoal(patrol));
            if (pathComplete(follower))
            {
                patrol.headingToSecond = !patrol.headingToSecond;
                clearPath(follower);
            }
        }
    }

    std::optional<NpcState> nextCowardState(NpcState state, const NpcFacts& facts)
    {
        switch (state)
        {
        case NpcState::Flee:
            if (facts.targetInBiteRange)
            {
                return NpcState::Bite;
            }
            if (!facts.targetKnown && facts.targetLostElapsed >= RecoverySeconds)
            {
                return patrolOrIdle(facts);
            }
            return std::nullopt;
        case NpcState::Bite:
            if (!facts.biteReady || facts.stateElapsed <= 0.0F)
            {
                return std::nullopt;
            }
            return facts.targetKnown ? NpcState::Flee : patrolOrIdle(facts);
        default:
            // Idle and Patrol share these decisions. Other tactics' states also come
            // here if supplied: flee from a nearby visible threat, otherwise return
            // to Patrol or Idle.
            if (facts.targetVisible && facts.targetWithinStandoffDistance)
            {
                return NpcState::Flee;
            }
            if (state != patrolOrIdle(facts))
            {
                return patrolOrIdle(facts);
            }
            return std::nullopt;
        }
    }

    void enterCowardState(Actor& actor, NpcState state)
    {
        if (state == NpcState::Bite)
        {
            actor.intentions.primaryAttackPressed = true;
        }
    }

    void updateCowardState(
        const NpcUpdate& update,
        Actor& actor,
        const NpcBrain& brain,
        PathFollower& follower,
        const Actor* target,
        NpcState state,
        float)
    {
        switch (state)
        {
        case NpcState::Patrol:
            updatePatrolState(update, actor, follower);
            break;
        case NpcState::Flee:
            updateFleeState(update, actor, brain, follower, target);
            break;
        case NpcState::Bite:
            aimToward(actor, brain.lastKnownTargetFeet);
            break;
        default:
            break;
        }
    }
}
