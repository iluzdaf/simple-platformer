#include "frame_axes.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "simple_platformer/timing/frame_profile.hpp"

namespace simple_platformer
{
    void FrameAxes::fit(Axis& axis, float worstSeconds, std::size_t capacity)
    {
        const float needed = std::max(axis.floor, worstSeconds * 1000.0F * AxisHeadroom);
        if (needed >= axis.top)
        {
            axis.top = needed;
            axis.framesUnneeded = 0;
            return;
        }
        ++axis.framesUnneeded;
        if (axis.framesUnneeded > capacity)
        {
            axis.top = needed;
            axis.framesUnneeded = 0;
        }
    }

    void FrameAxes::fitTo(const FrameHistory& history)
    {
        if (history.size() == 0)
        {
            return;
        }
        fit(frame, history.worst().frameSeconds, history.capacity());
        const std::vector<float> simulationSeconds = history.simulationSecondsOldestFirst();
        fit(simulation,
            *std::max_element(simulationSeconds.begin(), simulationSeconds.end()),
            history.capacity());
    }

    float FrameAxes::frameTopMilliseconds() const
    {
        return frame.top;
    }

    float FrameAxes::simulationTopMilliseconds() const
    {
        return simulation.top;
    }
}
