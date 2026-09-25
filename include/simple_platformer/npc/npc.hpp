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
        Shoot
    };

    struct NpcBrain
    {
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
    };

    struct Patrol
    {
        glm::vec2 firstFeet = {0.0F, 0.0F};
        glm::vec2 secondFeet = {0.0F, 0.0F};
        bool headingToSecond = true;
    };
}
