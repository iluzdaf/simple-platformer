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

    // Entering a state resets its time and path. Charge commits its direction;
    // Bite requests an attack once on entry.
    void enterNpcState(Actor& actor, NpcBrain& brain, PathFollower& follower, NpcState state);

    // This tick's intentions for the selected state. The target is the living
    // remembered target, if any, and the state has been running for stateElapsed.
    void updateNpcState(
        const NpcUpdate& update,
        Actor& actor,
        NpcBrain& brain,
        PathFollower& follower,
        const Actor* target,
        NpcState state,
        float stateElapsed);
}
