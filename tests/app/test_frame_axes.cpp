#include <catch2/catch_test_macros.hpp>

#include "debug/frame_axes.hpp"
#include "simple_platformer/timing/frame_profile.hpp"
#include "support/require_near.hpp"

namespace
{
    using simple_platformer::AxisHeadroom;
    using simple_platformer::FrameAxes;
    using simple_platformer::FrameAxisFloorMilliseconds;
    using simple_platformer::FrameHistory;
    using simple_platformer::FrameProfile;
    using simple_platformer::SimulationAxisFloorMilliseconds;

    FrameProfile frameTaking(float frameSeconds, float simulationSeconds)
    {
        FrameProfile frame;
        frame.frameSeconds = frameSeconds;
        frame.simulationSeconds = simulationSeconds;
        frame.simulationTicks = 1;
        return frame;
    }
}

TEST_CASE("The plot's axes start at their floors and grow to the worst frame", "[debug][profile]")
{
    FrameAxes axes;
    REQUIRE(axes.frameTopMilliseconds() == FrameAxisFloorMilliseconds);
    REQUIRE(axes.simulationTopMilliseconds() == SimulationAxisFloorMilliseconds);

    // Frames under the floors leave the axes where they are, an empty history too.
    FrameHistory history(2);
    axes.widenTo(history);
    history.push(frameTaking(0.010F, 0.00002F));
    axes.widenTo(history);
    REQUIRE(axes.frameTopMilliseconds() == FrameAxisFloorMilliseconds);
    REQUIRE(axes.simulationTopMilliseconds() == SimulationAxisFloorMilliseconds);

    // Each axis grows on its own, to its worst frame with headroom.
    history.push(frameTaking(0.050F, 0.00002F));
    axes.widenTo(history);
    REQUIRE_NEAR(axes.frameTopMilliseconds(), 50.0F * AxisHeadroom);
    REQUIRE(axes.simulationTopMilliseconds() == SimulationAxisFloorMilliseconds);
    history.push(frameTaking(0.010F, 0.0002F));
    axes.widenTo(history);
    REQUIRE_NEAR(axes.frameTopMilliseconds(), 50.0F * AxisHeadroom);
    REQUIRE_NEAR(axes.simulationTopMilliseconds(), 0.2F * AxisHeadroom);
}

TEST_CASE("The plot's axes never shrink once the worst frame has left", "[debug][profile]")
{
    FrameAxes axes;
    FrameHistory history(2);
    history.push(frameTaking(0.050F, 0.0002F));
    axes.widenTo(history);
    for (int frame = 0; frame < 3; ++frame)
    {
        history.push(frameTaking(0.010F, 0.00002F));
        axes.widenTo(history);
    }
    REQUIRE(history.worst().frameSeconds == 0.010F);
    REQUIRE_NEAR(axes.frameTopMilliseconds(), 50.0F * AxisHeadroom);
    REQUIRE_NEAR(axes.simulationTopMilliseconds(), 0.2F * AxisHeadroom);
}
