#include "frame_axes.hpp"

#include <algorithm>
#include <vector>

#include "simple_platformer/timing/frame_profile.hpp"

namespace simple_platformer
{
    namespace
    {
        float widened(float top, float worstSeconds)
        {
            return std::max(top, worstSeconds * 1000.0F * AxisHeadroom);
        }
    }

    void FrameAxes::widenTo(const FrameHistory& history)
    {
        if (history.size() == 0)
        {
            return;
        }
        frameTop = widened(frameTop, history.worst().frameSeconds);
        const std::vector<float> simulation = history.simulationSecondsOldestFirst();
        simulationTop =
            widened(simulationTop, *std::max_element(simulation.begin(), simulation.end()));
    }

    float FrameAxes::frameTopMilliseconds() const
    {
        return frameTop;
    }

    float FrameAxes::simulationTopMilliseconds() const
    {
        return simulationTop;
    }
}
