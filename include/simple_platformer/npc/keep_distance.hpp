#pragma once

#include <optional>
#include "simple_platformer/npc/npc.hpp"

namespace simple_platformer
{
    struct Actor;
    struct NpcFacts;
    struct NpcUpdate;
    struct PathFollower;

    // KeepDistance normally chooses Idle, Patrol, Chase, Shoot, Bite, Retreat, or Watch.
    std::optional<NpcState> nextKeepDistanceState(NpcState state, const NpcFacts& facts);
    void enterKeepDistanceState(Actor& actor, NpcState state);
    void updateKeepDistanceState(
        const NpcUpdate& update,
        Actor& actor,
        const NpcBrain& brain,
        PathFollower& follower,
        const Actor* target,
        NpcState state,
        float stateElapsed);
}
