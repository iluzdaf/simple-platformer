#pragma once

#include <vector>

#include "simple_platformer/input/input_state.hpp"

namespace simple_platformer
{
    struct InputStep
    {
        float duration = 0.0F;
        InputIntentions intentions;
    };

    using InputProgram = std::vector<InputStep>;

    float durationOf(const InputProgram& program);
    InputIntentions replayInput(const InputProgram& program, float elapsed);
}
