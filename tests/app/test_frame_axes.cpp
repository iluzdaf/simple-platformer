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
    axes.fitTo(history);
    history.push(frameTaking(0.010F, 0.00002F));
    axes.fitTo(history);
    REQUIRE(axes.frameTopMilliseconds() == FrameAxisFloorMilliseconds);
    REQUIRE(axes.simulationTopMilliseconds() == SimulationAxisFloorMilliseconds);

    // Each axis grows on its own, to its worst frame with headroom.
    history.push(frameTaking(0.050F, 0.00002F));
    axes.fitTo(history);
    REQUIRE_NEAR(axes.frameTopMilliseconds(), 50.0F * AxisHeadroom);
    REQUIRE(axes.simulationTopMilliseconds() == SimulationAxisFloorMilliseconds);
    history.push(frameTaking(0.010F, 0.0002F));
    axes.fitTo(history);
    REQUIRE_NEAR(axes.frameTopMilliseconds(), 50.0F * AxisHeadroom);
    REQUIRE_NEAR(axes.simulationTopMilliseconds(), 0.2F * AxisHeadroom);
}

TEST_CASE(
    "The plot's axes come down a full history after the worst frame has left",
    "[debug][profile]")
{
    FrameAxes axes;
    FrameHistory history(3);
    history.push(frameTaking(0.050F, 0.0002F));
    axes.fitTo(history);
    REQUIRE_NEAR(axes.frameTopMilliseconds(), 50.0F * AxisHeadroom);

    // Two more frames and the spike is still in the history; a third pushes it out,
    // and the axes hold for as many frames again before fitting what is left.
    const auto pushSmallFrame = [&]
    {
        history.push(frameTaking(0.010F, 0.00002F));
        axes.fitTo(history);
    };
    for (int frame = 0; frame < 3 + 2; ++frame)
    {
        pushSmallFrame();
        REQUIRE_NEAR(axes.frameTopMilliseconds(), 50.0F * AxisHeadroom);
        REQUIRE_NEAR(axes.simulationTopMilliseconds(), 0.2F * AxisHeadroom);
    }
    REQUIRE(history.worst().frameSeconds == 0.010F);
    pushSmallFrame();
    REQUIRE(axes.frameTopMilliseconds() == FrameAxisFloorMilliseconds);
    REQUIRE(axes.simulationTopMilliseconds() == SimulationAxisFloorMilliseconds);

    // A spike while the axes are holding starts the wait over.
    history.push(frameTaking(0.040F, 0.00002F));
    axes.fitTo(history);
    for (int frame = 0; frame < 3 + 2; ++frame)
    {
        pushSmallFrame();
    }
    REQUIRE_NEAR(axes.frameTopMilliseconds(), 40.0F * AxisHeadroom);
    pushSmallFrame();
    REQUIRE(axes.frameTopMilliseconds() == FrameAxisFloorMilliseconds);
}
