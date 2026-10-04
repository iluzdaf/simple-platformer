#include "simple_platformer/npc/pursuer.hpp"

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
        NpcState patrolOrIdle(const NpcFacts& facts)
        {
            return facts.hasPatrol ? NpcState::Patrol : NpcState::Idle;
        }

        NpcState lostTarget(const NpcFacts& facts)
        {
            if (!facts.searches)
            {
                return patrolOrIdle(facts);
            }
            return NpcState::Search;
        }

        std::optional<NpcState> pursuit(const NpcFacts& facts)
        {
            if (!facts.targetKnown)
            {
                return std::nullopt;
            }
            if (facts.targetInBiteRange)
            {
                return NpcState::Bite;
            }
            if (facts.targetInSights)
            {
                return NpcState::Shoot;
            }
            return NpcState::Chase;
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

        void updateChaseState(
            const NpcUpdate& update,
            Actor& actor,
            const NpcBrain& brain,
            PathFollower& follower,
            const Actor* target)
        {
            if (target == nullptr)
            {
                throw std::logic_error("A chasing NPC has no target");
            }

            actor.intentions =
                intentionsToReach(update, actor, follower, brain.lastKnownTargetFeet);
            aimToward(actor, brain.lastKnownTargetFeet);
        }

        // The tactic leaves Shoot when sight is lost.
        void updateShootState(Actor& actor, const Actor* target)
        {
            if (target == nullptr)
            {
                throw std::logic_error("A shooting NPC has no target");
            }
            actor.intentions.aimDirection =
                centerOf(target->body.bounds) - centerOf(actor.body.bounds);
            actor.intentions.primaryAttackPressed = true;
        }

        // Walk to the remembered position, then look about if there or unreachable.
        void updateSearchState(
            const NpcUpdate& update,
            Actor& actor,
            const NpcBrain& brain,
            PathFollower& follower,
            float stateElapsed)
        {
            actor.intentions =
                intentionsToReach(update, actor, follower, brain.lastKnownTargetFeet);
            if (!follower.path.has_value() || pathComplete(follower))
            {
                lookAbout(actor, brain, stateElapsed);
            }
        }
    }

    std::optional<NpcState> nextPursuerState(NpcState state, const NpcFacts& facts)
    {
        const std::optional<NpcState> pursuing = pursuit(facts);
        // Supplied states from other tactics reuse these decisions: Flee/Sleep/Charge/
        // Stunned use Idle, Retreat uses Chase/Shoot, and Watch uses Search.
        switch (state)
        {
        case NpcState::Flee:
        case NpcState::Sleep:
        case NpcState::Charge:
        case NpcState::Stunned:
        case NpcState::Idle:
            if (pursuing.has_value())
            {
                return pursuing;
            }
            if (facts.hasPatrol)
            {
                return NpcState::Patrol;
            }
            return std::nullopt;
        case NpcState::Patrol:
            return pursuing;
        case NpcState::Chase:
        case NpcState::Shoot:
        case NpcState::Retreat:
            if (!pursuing.has_value())
            {
                return lostTarget(facts);
            }
            return *pursuing != state ? pursuing : std::nullopt;
        case NpcState::Search:
        case NpcState::Watch:
            if (pursuing.has_value())
            {
                return pursuing;
            }
            return facts.searchTimeUp ? std::optional(patrolOrIdle(facts)) : std::nullopt;
        case NpcState::Bite:
            // Ready on entry means combat has not seen the request yet. Wait for a later
            // decision before treating Ready as a finished bite. Chase before biting again.
            if (!facts.biteReady || facts.stateElapsed <= 0.0F)
            {
                return std::nullopt;
            }
            return facts.targetKnown ? NpcState::Chase : lostTarget(facts);
        }
        return std::nullopt;
    }

    void enterPursuerState(Actor& actor, NpcState state)
    {
        if (state == NpcState::Bite)
        {
            actor.intentions.primaryAttackPressed = true;
        }
    }

    void updatePursuerState(
        const NpcUpdate& update,
        Actor& actor,
        const NpcBrain& brain,
        PathFollower& follower,
        const Actor* target,
        NpcState state,
        float stateElapsed)
    {
        switch (state)
        {
        case NpcState::Patrol:
            updatePatrolState(update, actor, follower);
            break;
        case NpcState::Chase:
            updateChaseState(update, actor, brain, follower, target);
            break;
        case NpcState::Bite:
            aimToward(actor, brain.lastKnownTargetFeet);
            break;
        case NpcState::Shoot:
            updateShootState(actor, target);
            break;
        case NpcState::Search:
            updateSearchState(update, actor, brain, follower, stateElapsed);
            break;
        case NpcState::Watch:
            lookAbout(actor, brain, stateElapsed);
            break;
        default:
            break;
        }
    }
}
