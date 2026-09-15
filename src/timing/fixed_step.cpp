#include "simple_platformer/timing/fixed_step.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <stdexcept>

namespace simple_platformer
{
    FixedStep::FixedStep(double stepSeconds, double maximumFrameSeconds)
        : step(stepSeconds), maximumFrame(maximumFrameSeconds)
    {
        if (!std::isfinite(step) || step <= 0.0)
            throw std::invalid_argument("Fixed step must be finite and above zero");

        if (!std::isfinite(maximumFrame) || maximumFrame < 0.0)
            throw std::invalid_argument("Maximum frame time must be finite and non-negative");
    }

    FixedStepResult FixedStep::advance(
        double frameSeconds,
        const std::function<void(float)>& fixedUpdate)
    {
        if (!std::isfinite(frameSeconds))
            throw std::invalid_argument("Frame time must be finite");

        accumulator += std::clamp(frameSeconds, 0.0, maximumFrame);

        std::size_t updates = 0;
        while (accumulator >= step)
        {
            accumulator -= step;
            fixedUpdate(static_cast<float>(step));
            ++updates;
        }

        return {updates, accumulator / step};
    }

    void FixedStep::reset()
    {
        accumulator = 0.0;
    }

    double FixedStep::stepSeconds() const
    {
        return step;
    }

    double FixedStep::pendingSeconds() const
    {
        return accumulator;
    }
}
