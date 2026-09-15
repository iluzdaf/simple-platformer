#pragma once

#include <cstddef>
#include <functional>

namespace simple_platformer
{
    constexpr double FixedDeltaSeconds = 1.0 / 60.0;
    constexpr double MaximumFrameSeconds = 0.25;

    struct FixedStepResult
    {
        std::size_t updates = 0;
        double interpolation = 0.0;
    };

    class FixedStep
    {
    public:
        explicit FixedStep(
            double stepSeconds = FixedDeltaSeconds,
            double maximumFrameSeconds = MaximumFrameSeconds);

        FixedStepResult advance(double frameSeconds, const std::function<void(float)>& fixedUpdate);

        void reset();
        double stepSeconds() const;
        double pendingSeconds() const;

    private:
        double step;
        double maximumFrame;
        double accumulator = 0.0;
    };
}
