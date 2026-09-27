#pragma once

#include <vector>

#include "simple_platformer/input/input_state.hpp"

namespace simple_platformer
{
    // One set of intentions held for a duration. Replay returns edge flags on every
    // call within that duration, so callers must choose their step lengths accordingly.
    struct InputStep
    {
        float duration = 0.0F;
        InputIntentions intentions;
    };

    using InputProgram = std::vector<InputStep>;

    // How long the whole program takes. Every step must last a finite, positive time.
    float durationOf(const InputProgram& program);
    // The intentions to hold this far into the program, or none once it has run out.
    InputIntentions replayInput(const InputProgram& program, float elapsed);
}
