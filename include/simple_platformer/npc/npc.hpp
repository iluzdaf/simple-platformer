#pragma once

#include <optional>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor_id.hpp"

namespace simple_platformer
{
    enum class NpcState
    {
        Idle,
        Patrol,
        Chase,
        Bite,
        Shoot,
        Search,
        Retreat,
        Watch
    };

    // The brain's policy, asked wherever the transition table makes a choice: what to do
    // about a known target, and where a lost one leaves the NPC. A Pursuer closes in,
    // attacks with what reaches, and searches where it lost its target. A KeepDistance
    // NPC backs away from a target nearer than its standoff, so a ranged NPC keeps its
    // range, and watches from where it stands rather than walk to where the target was.
    // A tactic chooses between states that exist; it never adds behaviour.
    enum class NpcTactic
    {
        Pursuer,
        KeepDistance
    };

    struct NpcBrain
    {
        NpcTactic tactic = NpcTactic::Pursuer;
        NpcState state = NpcState::Idle;
        float stateElapsed = 0.0F;
        std::optional<ActorId> target;
        glm::vec2 lastSeenTargetFeet = {0.0F, 0.0F};
        float targetMemoryRemaining = 0.0F;
        bool targetVisible = false;
    };

    struct NpcSenses
    {
        float noticeDistance = 96.0F;
        float targetMemoryDuration = 1.5F;
        // How long a lost target is searched for before the NPC returns to its routine.
        // Zero sends it straight back.
        float searchDuration = 2.0F;
        // How near a target may come before it counts as too close, which a KeepDistance
        // brain or a machine answers with a retreat.
        float standoffDistance = 48.0F;
    };

    struct Patrol
    {
        glm::vec2 firstFeet = {0.0F, 0.0F};
        glm::vec2 secondFeet = {0.0F, 0.0F};
        bool headingToSecond = true;
    };
}
