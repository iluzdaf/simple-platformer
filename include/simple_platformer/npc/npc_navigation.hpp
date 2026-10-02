#pragma once

#include <glm/vec2.hpp>

#include "simple_platformer/input/input_state.hpp"

namespace simple_platformer
{
    struct Actor;
    struct NpcUpdate;
    struct PathFollower;

    // Returns movement intentions towards the goal, planning a path when needed.
    // Replans after a tile break, a large enough goal change, or displacement from a
    // finished path. A caller that also aims sets the aim afterwards.
    InputIntentions intentionsToReach(
        const NpcUpdate& update,
        Actor& actor,
        PathFollower& follower,
        glm::vec2 goalFeet);
}
