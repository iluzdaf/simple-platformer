#pragma once

#include <vector>

#include "simple_platformer/input/input_state.hpp"

namespace simple_platformer
{
    // Intentions to hold for a stretch of time. A jump or a fall is recorded as a run of
    // these when its connection is simulated, and the path follower replays them.
    struct InputStep
    {
        float duration = 0.0F;
        InputIntentions intentions;
    };

    // The steps in the order they are held.
    using InputProgram = std::vector<InputStep>;

    // How long the whole program takes. Every step must last a finite, positive time.
    float durationOf(const InputProgram& program);
    // The intentions to hold this far into the program, or none once it has run out.
    InputIntentions replayInput(const InputProgram& program, float elapsed);
}
