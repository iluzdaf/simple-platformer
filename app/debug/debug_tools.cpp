#include "debug_tools.hpp"

#include "debug_overlay.hpp"
#include "debug_overlay_ui.hpp"
#include "frame_profile_ui.hpp"
#include "machine_graph_ui.hpp"

#include <optional>

#include "simple_platformer/timing/frame_profile.hpp"

namespace simple_platformer
{
    void drawDebugTools(
        DebugTools& tools,
        const FrameProfile& profile,
        const DebugOverlay& overlay,
        const std::optional<WindowViewport>& viewport,
        bool showFrameProfileDetails)
    {
        drawDebugOverlay(overlay, viewport);
        drawMachineGraph(tools.machineEditors, overlay.machine);
        tools.frameHistory.push(profile);
        drawFrameProfile(
            tools.frameHistory, tools.frameSelection, tools.frameAxes, showFrameProfileDetails);
    }
}
