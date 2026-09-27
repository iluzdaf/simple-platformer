#pragma once

#include <optional>

#include "simple_platformer/npc/npc.hpp"

namespace simple_platformer
{
    // The policy's snapshot for this update, gathered from perception, brain memory,
    // and other actor components, so transitions read nothing else.
    struct NpcFacts
    {
        // A living target is remembered, seen or not.
        bool targetKnown = false;
        bool targetVisible = false;
        // The target is visible and inside the bite's hitbox.
        bool targetInBiteRange = false;
        bool biteReady = false;
        // The target is visible and the NPC has a ranged weapon.
        bool targetInSights = false;
        // The living target's last known feet are strictly within the senses'
        // standoff distance of the NPC's current feet.
        bool targetWithinStandoffDistance = false;
        bool heardLanding = false;
        // Current geometry of the living remembered target, even when it is not visible.
        // Range and shared ground are independent; policy chooses how to combine them.
        bool targetOnSameRun = false;
        bool targetWithinNoticeDistance = false;
        bool movementBlocked = false;
        bool hasPatrol = false;
        // The NPC searches for a lost target at all, and its search has run its time.
        bool searches = false;
        bool searchTimeUp = false;
        float stateElapsed = 0.0F;
    };

    // The state to enter from this one given the facts, or nothing to stay. Every
    // transition the NPC makes is a branch here, and none of them act. The tactic is
    // asked wherever the table makes a choice: the pursuit of a known target, and where
    // a lost one leaves the NPC. Every other transition is shared.
    std::optional<NpcState> nextNpcState(NpcTactic tactic, NpcState state, const NpcFacts& facts);
}
