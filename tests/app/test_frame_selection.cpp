#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>
#include <vector>

#include "debug/frame_selection.hpp"
#include "simple_platformer/timing/frame_profile.hpp"
#include "support/require_near.hpp"

namespace
{
    using simple_platformer::frameAtPlotFraction;
    using simple_platformer::FrameHistory;
    using simple_platformer::frameNearestPlotFraction;
    using simple_platformer::FrameProfile;
    using simple_platformer::FrameSelection;

    FrameProfile frameTaking(float seconds)
    {
        FrameProfile frame;
        frame.frameSeconds = seconds;
        return frame;
    }

    FrameHistory historyOf(const std::vector<float>& frameSeconds)
    {
        FrameHistory history(4);
        for (const float seconds : frameSeconds)
        {
            history.push(frameTaking(seconds));
        }
        return history;
    }
}

TEST_CASE(
    "A click picks the frame nearest its column, or nothing past the last",
    "[debug][profile]")
{
    // Ten frames plotted at x = 0..9 on an axis 120 wide.
    REQUIRE(frameAtPlotFraction(0.0F, 120, 10) == 0);
    REQUIRE(frameAtPlotFraction(0.4F / 120.0F, 120, 10) == 0);
    REQUIRE(frameAtPlotFraction(0.6F / 120.0F, 120, 10) == 1);
    REQUIRE(frameAtPlotFraction(9.0F / 120.0F, 120, 10) == 9);
    REQUIRE_FALSE(frameAtPlotFraction(10.0F / 120.0F, 120, 10).has_value());
    REQUIRE_FALSE(frameAtPlotFraction(1.0F, 120, 120).has_value());
    REQUIRE_FALSE(frameAtPlotFraction(-0.1F, 120, 10).has_value());
    REQUIRE_FALSE(frameAtPlotFraction(0.0F, 120, 0).has_value());
}

TEST_CASE("A drag scrubs to the nearest frame and stops at the ends", "[debug][profile]")
{
    REQUIRE(frameNearestPlotFraction(0.6F / 120.0F, 120, 10) == 1);
    REQUIRE(frameNearestPlotFraction(-0.5F, 120, 10) == 0);
    REQUIRE(frameNearestPlotFraction(9.0F / 120.0F, 120, 10) == 9);
    // Dragged past the last frame or off the plot, it holds the last one.
    REQUIRE(frameNearestPlotFraction(0.5F, 120, 10) == 9);
    REQUIRE(frameNearestPlotFraction(1.5F, 120, 10) == 9);
    REQUIRE_THROWS_AS(frameNearestPlotFraction(0.0F, 120, 0), std::invalid_argument);
}

TEST_CASE("Selecting a frame keeps the history as it was until cleared", "[debug][profile]")
{
    FrameHistory live = historyOf({0.010F, 0.020F, 0.030F});
    FrameSelection selection;
    REQUIRE(selection.kept() == nullptr);
    REQUIRE_FALSE(selection.selectedIndex().has_value());

    selection.select(live, 1);
    REQUIRE(selection.selectedIndex() == 1);
    REQUIRE_NEAR(selection.selectedFrame().frameSeconds, 0.020F);

    // The live history moves on; the selection does not.
    live.push(frameTaking(0.040F));
    REQUIRE(selection.kept() != nullptr);
    REQUIRE(
        selection.kept()->frameSecondsOldestFirst() == std::vector<float>{0.010F, 0.020F, 0.030F});

    // Another frame of the kept history can be selected without losing it, and the
    // selected one again changes nothing.
    selection.select(live, 2);
    selection.select(live, 2);
    REQUIRE(selection.selectedIndex() == 2);
    REQUIRE_NEAR(selection.selectedFrame().frameSeconds, 0.030F);
    REQUIRE(selection.kept()->size() == 3);

    // Clearing lets the live history show again.
    selection.clear();
    REQUIRE_FALSE(selection.selectedIndex().has_value());
    REQUIRE(selection.kept() == nullptr);
}

TEST_CASE("Selecting a frame that was not plotted is rejected", "[debug][profile]")
{
    const FrameHistory live = historyOf({0.010F, 0.020F, 0.030F});
    FrameSelection selection;
    REQUIRE_THROWS_AS(selection.selectedFrame(), std::logic_error);
    REQUIRE_THROWS_AS(selection.select(live, 3), std::out_of_range);
    REQUIRE_FALSE(selection.selectedIndex().has_value());

    selection.select(live, 0);
    REQUIRE_THROWS_AS(selection.select(live, 3), std::out_of_range);
    REQUIRE(selection.selectedIndex() == 0);
}
