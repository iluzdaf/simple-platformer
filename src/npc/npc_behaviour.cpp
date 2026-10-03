#include "simple_platformer/npc/npc_behaviour.hpp"

#include <optional>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/charger.hpp"
#include "simple_platformer/npc/coward.hpp"
#include "simple_platformer/npc/keep_distance.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_facts.hpp"
#include "simple_platformer/npc/npc_update.hpp"
#include "simple_platformer/npc/pursuer.hpp"

namespace simple_platformer
{
    namespace
    {
        constexpr float SearchTurnSeconds = 0.5F;
    }

    void aimToward(Actor& actor, glm::vec2 targetFeet)
    {
        actor.intentions.aimDirection = targetFeet - feetOf(actor.body.bounds);
    }

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

    std::optional<NpcState> nextNpcState(NpcTactic tactic, NpcState state, const NpcFacts& facts)
    {
        switch (tactic)
        {
        case NpcTactic::Pursuer:
            return nextPursuerState(state, facts);
        case NpcTactic::KeepDistance:
            return nextKeepDistanceState(state, facts);
        case NpcTactic::Coward:
            return nextCowardState(state, facts);
        case NpcTactic::Charger:
            return nextChargerState(state, facts);
        }
        return std::nullopt;
    }

    void enterNpcState(Actor& actor, NpcBrain& brain, PathFollower& follower, NpcState state)
    {
        brain.state = state;
        brain.stateElapsed = 0.0F;
        clearPath(follower);
        switch (brain.tactic)
        {
        case NpcTactic::Pursuer:
            enterPursuerState(actor, state);
            break;
        case NpcTactic::KeepDistance:
            enterKeepDistanceState(actor, state);
            break;
        case NpcTactic::Coward:
            enterCowardState(actor, state);
            break;
        case NpcTactic::Charger:
            enterChargerState(actor, brain, state);
            break;
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
        switch (brain.tactic)
        {
        case NpcTactic::Pursuer:
            updatePursuerState(update, actor, brain, follower, target, state, stateElapsed);
            break;
        case NpcTactic::KeepDistance:
            updateKeepDistanceState(update, actor, brain, follower, target, state, stateElapsed);
            break;
        case NpcTactic::Coward:
            updateCowardState(update, actor, brain, follower, target, state, stateElapsed);
            break;
        case NpcTactic::Charger:
            updateChargerState(update, actor, brain, target, state);
            break;
        }
    }
}
