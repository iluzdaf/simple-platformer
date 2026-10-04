#pragma once

#include <optional>
#include "simple_platformer/npc/npc.hpp"

namespace simple_platformer
{
    struct Actor;
    struct NpcFacts;
    struct NpcUpdate;
    struct PathFollower;

    // Pursuer normally chooses Idle, Patrol, Chase, Shoot, Bite, or Search.
    std::optional<NpcState> nextPursuerState(NpcState state, const NpcFacts& facts);
    void enterPursuerState(Actor& actor, NpcState state);
    void updatePursuerState(
        const NpcUpdate& update,
        Actor& actor,
        const NpcBrain& brain,
        PathFollower& follower,
        const Actor* target,
        NpcState state,
        float stateElapsed);
}
