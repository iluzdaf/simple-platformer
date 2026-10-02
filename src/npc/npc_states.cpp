#include "simple_platformer/npc/npc_states.hpp"

#include <stdexcept>

#include <glm/geometric.hpp>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/platformer_cells.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_navigation.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/npc/npc_update.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    namespace
    {
        constexpr float SearchTurnSeconds = 0.5F;

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

        // Looking about is an aim that turns every SearchTurnSeconds, first towards where
        // the target was last known to be.
        void lookAbout(Actor& actor, const NpcBrain& brain, float stateElapsed)
        {
            const float toward = brain.lastKnownTargetFeet.x - feetOf(actor.body.bounds).x;
            float side = toward < 0.0F ? -1.0F : 1.0F;
            const int turns = static_cast<int>(stateElapsed / SearchTurnSeconds);
            if (turns % 2 == 1)
            {
                side = -side;
            }
            actor.intentions.aimDirection = {side, 0.0F};
        }

        // A search finishes the walk to where the target was last known to be, and looks about
        // once it is there or cannot get there.
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

        // Retreat aims at the last known target feet and requests a primary attack.
        // A walker stops at a ledge instead of stepping off it.
        void updateRetreatState(const NpcUpdate& update, Actor& actor, const NpcBrain& brain)
        {
            const glm::vec2 feet = feetOf(actor.body.bounds);
            glm::vec2 away = feet - brain.lastKnownTargetFeet;
            if (actor.platformerMovement.has_value())
            {
                const float side = away.x < 0.0F ? -1.0F : 1.0F;
                away = {side, 0.0F};
                const Cell ahead = cellAtFeet(
                    update.map.tileSize(), feet + glm::vec2{side * actor.body.bounds.size.x, 0.0F});
                if (!canStandAt(update.map, ahead, actor.body.bounds.size))
                {
                    away = {0.0F, 0.0F};
                }
            }
            actor.intentions.direction = away;
            aimToward(actor, brain.lastKnownTargetFeet);
            actor.intentions.primaryAttackPressed = true;
        }
    }

    void aimToward(Actor& actor, glm::vec2 targetFeet)
    {
        actor.intentions.aimDirection = targetFeet - feetOf(actor.body.bounds);
    }

    void enterNpcState(Actor& actor, NpcBrain& brain, PathFollower& follower, NpcState state)
    {
        brain.state = state;
        brain.stateElapsed = 0.0F;
        clearPath(follower);
        if (state == NpcState::Charge)
        {
            brain.chargeDirection =
                brain.lastKnownTargetFeet.x < feetOf(actor.body.bounds).x ? -1.0F : 1.0F;
        }
        if (state == NpcState::Bite)
        {
            actor.intentions.primaryAttackPressed = true;
        }
    }

    void updateNpcState(
        const NpcUpdate& update,
        Actor& actor,
        NpcBrain& brain,
        PathFollower& follower,
        const Actor* target,
        NpcState state,
        float stateElapsed)
    {
        switch (state)
        {
        case NpcState::Flee:
            updateFleeState(update, actor, brain, follower, target);
            break;
        case NpcState::Sleep:
            break;
        case NpcState::Charge:
            actor.intentions.direction = {brain.chargeDirection, 0.0F};
            actor.intentions.avoidLedges = true;
            actor.intentions.contactDamage = true;
            break;
        case NpcState::Stunned:
            if (target != nullptr && actor.platformerMovement.has_value() &&
                actor.platformerMovement->grounded && target->platformerMovement.has_value() &&
                target->platformerMovement->grounded &&
                onSameGroundRun(update.map, actor.body.bounds, target->body.bounds) &&
                actor.senses.has_value() &&
                glm::distance(feetOf(actor.body.bounds), feetOf(target->body.bounds)) <=
                    actor.senses->noticeDistance)
            {
                aimToward(actor, brain.lastKnownTargetFeet);
            }
            break;
        case NpcState::Idle:
            break;
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
        case NpcState::Retreat:
            updateRetreatState(update, actor, brain);
            break;
        case NpcState::Watch:
            lookAbout(actor, brain, stateElapsed);
            break;
        }
    }
}
