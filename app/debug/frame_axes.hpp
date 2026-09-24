#pragma once

#include <cstddef>

#include "simple_platformer/timing/fixed_step.hpp"

namespace simple_platformer
{
    class FrameHistory;

    // Where each of the plot's vertical axes starts. The frame axis keeps the 60 Hz
    // budget line in the lower half; the simulation axis fits an ordinary frame of this
    // project, which simulates in a fraction of it.
    constexpr float FrameAxisFloorMilliseconds = static_cast<float>(FixedDeltaSeconds) * 2000.0F;
    constexpr float SimulationAxisFloorMilliseconds = 0.06F;
    // How far an axis stretches over the worst frame it has grown to fit.
    constexpr float AxisHeadroom = 1.1F;

    // The tops of the plot's two vertical axes: frame time on the left, the simulation's
    // stack on the right. Each starts at its floor and grows at once to fit the worst
    // frame in the history with headroom, so a spike is never cut off. It comes down
    // again only slowly: after the spike has left the history, the axis holds for
    // another full turn of the history before fitting what is left, so the scale does
    // not jump about under the reader.
    class FrameAxes
    {
    public:
        // Fits the axes to this history's frames, growing at once and shrinking late.
        void fitTo(const FrameHistory& history);

        float frameTopMilliseconds() const;
        float simulationTopMilliseconds() const;

    private:
        struct Axis
        {
            float floor;
            float top;
            // Frames in a row for which the top has been more than the history needed.
            std::size_t framesUnneeded = 0;
        };

        static void fit(Axis& axis, float worstSeconds, std::size_t capacity);

        Axis frame{FrameAxisFloorMilliseconds, FrameAxisFloorMilliseconds};
        Axis simulation{SimulationAxisFloorMilliseconds, SimulationAxisFloorMilliseconds};
    };
}
