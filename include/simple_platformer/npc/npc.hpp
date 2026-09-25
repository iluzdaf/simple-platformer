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
        Retreat
    };

    // The brain's policy, asked wherever the transition table makes a choice; today that
    // is what to do about a known target. A Pursuer closes in and attacks with what
    // reaches. A KeepDistance NPC does the same but backs away from a target nearer than
    // its standoff, so a ranged NPC keeps its range. A tactic chooses between states that
    // exist; it never adds behaviour.
    enum class NpcTactic
    {
        Pursuer,
        KeepDistance
    };

    struct NpcBrain
    {
        NpcTactic tactic = NpcTactic::Pursuer;
        // How near a KeepDistance NPC lets its target come before it retreats.
        float standoffDistance = 48.0F;
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
    };

    struct Patrol
    {
        glm::vec2 firstFeet = {0.0F, 0.0F};
        glm::vec2 secondFeet = {0.0F, 0.0F};
        bool headingToSecond = true;
    };
}
