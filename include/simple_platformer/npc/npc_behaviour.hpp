#pragma once

#include <optional>

#include <glm/vec2.hpp>

#include "simple_platformer/npc/npc.hpp"

namespace simple_platformer
{
    struct Actor;
    struct NpcFacts;
    struct NpcUpdate;
    struct PathFollower;

    // Looking at the target is an aim, like everything else an actor intends; the
    // movement update turns it into a facing.
    void aimToward(Actor& actor, glm::vec2 targetFeet);

    // Turns aim periodically, starting towards the remembered target.
    void lookAbout(Actor& actor, const NpcBrain& brain, float stateElapsed);

    // Returns the state to enter, or nothing to stay. Uses only the tactic, current state,
    // and facts; it does not change the actor or issue intentions.
    std::optional<NpcState> nextNpcState(NpcTactic tactic, NpcState state, const NpcFacts& facts);

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
