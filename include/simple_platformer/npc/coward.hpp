#pragma once

#include <optional>
#include "simple_platformer/npc/npc.hpp"

namespace simple_platformer
{
    struct Actor;
    struct NpcFacts;
    struct NpcUpdate;
    struct PathFollower;

    // Coward normally chooses Idle, Patrol, Flee, or Bite.
    std::optional<NpcState> nextCowardState(NpcState state, const NpcFacts& facts);
    void enterCowardState(Actor& actor, NpcState state);
    void updateCowardState(
        const NpcUpdate& update,
        Actor& actor,
        const NpcBrain& brain,
        PathFollower& follower,
        const Actor* target,
        NpcState state,
        float stateElapsed);
}
