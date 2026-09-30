#pragma once

#include <glm/vec2.hpp>

#include "simple_platformer/npc/npc.hpp"

namespace simple_platformer
{
    struct Actor;
    struct NpcUpdate;
    struct PathFollower;

    // Looking at the target is an aim, like everything else an actor intends; the
    // movement update turns it into a facing.
    void aimToward(Actor& actor, glm::vec2 targetFeet);

    // Every activity change drops the old path, and a bite is asked for once as its
    // activity is entered.
    void enterBuiltInActivity(Actor& actor, PathFollower& follower, NpcState state);

    // This tick's intentions for one of the engine's own activities, the states the tactic
    // table and machines both use. The target is the living remembered target, if any, and
    // the activity has been running for stateElapsed.
    void updateBuiltInActivity(
        const NpcUpdate& update,
        Actor& actor,
        NpcBrain& brain,
        PathFollower& follower,
        const Actor* target,
        NpcState state,
        float stateElapsed);
}
