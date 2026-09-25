#pragma once

#include <optional>

#include "simple_platformer/npc/npc.hpp"

namespace simple_platformer
{
    // What the brain decides on this update, gathered once from the senses' stamps, the
    // bite attack and the patrol, so the transitions read nothing else.
    struct NpcFacts
    {
        // A living target is remembered, seen or not.
        bool targetKnown = false;
        bool targetVisible = false;
        // The target is visible and inside the bite's hitbox.
        bool targetInBiteRange = false;
        // The NPC has a bite and it is ready to start.
        bool biteReady = false;
        // The target is visible and the NPC has a ranged weapon.
        bool canShootTarget = false;
        bool hasPatrol = false;
        float stateElapsed = 0.0F;
    };

    // The state to enter from this one given the facts, or nothing to stay. Every
    // transition the NPC makes is a branch here, and none of them act.
    std::optional<NpcState> nextNpcState(NpcState state, const NpcFacts& facts);
}
