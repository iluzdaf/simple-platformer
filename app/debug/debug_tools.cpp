#include "debug_tools.hpp"

#include "debug_overlay.hpp"
#include "debug_overlay_ui.hpp"
#include "frame_profile_ui.hpp"
#include "frame_selection.hpp"

#include <optional>

#include "simple_platformer/timing/frame_profile.hpp"

namespace simple_platformer
{
    FramePlotRequest drawDebugTools(
        DebugTools& tools,
        const FrameProfile& profile,
        const DebugOverlay& overlay,
        const std::optional<WindowViewport>& viewport,
        const DebugToolVisibility& visibility,
        bool paused)
    {
        if (visibility.worldAndCameraOverlay || visibility.actorText ||
            visibility.navigationConnectionsText)
        {
            drawDebugOverlay(
                overlay,
                viewport,
                visibility.worldAndCameraOverlay,
                visibility.actorText,
                visibility.navigationConnectionsText);
        }
        recordFrameForPlot(tools.frameHistory, tools.frameSelection, profile, paused);
        return drawFrameProfile(
            tools.frameHistory,
            tools.frameSelection,
            tools.frameAxes,
            visibility.frameProfileDetails);
    }
}
