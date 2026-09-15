#include "simple_platformer/navigation/input_program.hpp"

#include <cmath>
#include <stdexcept>

#include "simple_platformer/input/input_state.hpp"

namespace simple_platformer
{
    float durationOf(const InputProgram& program)
    {
        float duration = 0.0F;
        for (const InputStep& step : program)
        {
            if (!std::isfinite(step.duration) || step.duration <= 0.0F)
            {
                throw std::invalid_argument("Input program durations must be finite and positive");
            }
            duration += step.duration;
        }
        return duration;
    }

    InputIntentions replayInput(const InputProgram& program, float elapsed)
    {
        if (!std::isfinite(elapsed) || elapsed < 0.0F)
        {
            throw std::invalid_argument(
                "Input program elapsed time must be finite and non-negative");
        }

        float endsAt = 0.0F;
        for (const InputStep& step : program)
        {
            if (!std::isfinite(step.duration) || step.duration <= 0.0F)
            {
                throw std::invalid_argument("Input program durations must be finite and positive");
            }
            endsAt += step.duration;
            if (elapsed < endsAt)
            {
                return step.intentions;
            }
        }
        return {};
    }
}
