#pragma once

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
    // stack on the right. Each starts at its floor, grows to fit the worst frame seen
    // with headroom, and never shrinks, so a spike stretches the plot once and the scale
    // then holds still under the reader.
    class FrameAxes
    {
    public:
        // Widens either axis the frames of this history have outgrown.
        void widenTo(const FrameHistory& history);

        float frameTopMilliseconds() const;
        float simulationTopMilliseconds() const;

    private:
        float frameTop = FrameAxisFloorMilliseconds;
        float simulationTop = SimulationAxisFloorMilliseconds;
    };
}
