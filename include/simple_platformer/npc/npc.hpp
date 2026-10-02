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
        Watch,
        Flee,
        Sleep,
        Charge,
        Stunned
    };

    // The brain's policy, asked wherever the transition table makes a choice: what to do
    // about a known target, and where a lost one leaves the NPC. A Pursuer closes in,
    // attacks with what reaches, and searches where it lost its target. A KeepDistance
    // NPC backs away from a target nearer than its standoff, so a ranged NPC keeps its
    // range, and watches from where it stands rather than walk to where the target was.
    // Coward flees and bites when cornered. Charger wakes on a landing, commits to
    // a charge, and recovers after being blocked. Tactics select built-in states.
    enum class NpcTactic
    {
        Pursuer,
        KeepDistance,
        Coward,
        Charger
    };

    // Decision state and persistent knowledge, retained between sensing updates.
    struct NpcBrain
    {
        NpcTactic tactic = NpcTactic::Pursuer;
        NpcState state = NpcState::Idle;
        float stateElapsed = 0.0F;
        // Continuous time without a remembered living target, used when fleeing.
        float targetLostElapsed = 0.0F;
        // Chosen on entering Charge and retained until the charge ends.
        float chargeDirection = 1.0F;
        std::optional<ActorId> target;
        // Last observed target feet, refreshed by sight or an eligible noise.
        glm::vec2 lastKnownTargetFeet = {0.0F, 0.0F};
        float targetMemoryRemaining = 0.0F;
    };

    // Transient observations, replaced on every sensing update. Behaviour copies
    // these into NpcFacts alongside facts derived from other actor components.
    struct NpcPerception
    {
        bool targetVisible = false;
        // A player landing heard on this run during the latest sensing update.
        bool heardLanding = false;
    };

    struct NpcSenses
    {
        float noticeDistance = 96.0F;
        float targetMemoryDuration = 1.5F;
        // How long a lost target is searched for before the NPC returns to its routine.
        // Zero sends it straight back.
        float searchDuration = 2.0F;
        // Threshold for targetWithinStandoffDistance; tactics decide
        // what to do when the target crosses it.
        float standoffDistance = 48.0F;
    };

    struct Patrol
    {
        glm::vec2 firstFeet = {0.0F, 0.0F};
        glm::vec2 secondFeet = {0.0F, 0.0F};
        bool headingToSecond = true;
    };
}
