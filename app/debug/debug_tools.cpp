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
        const DebugToolVisibility& visibility)
    {
        if (tools.machineActor.has_value() &&
            (!overlay.machine.has_value() || overlay.machine->actor != *tools.machineActor))
        {
            tools.machineActor.reset();
        }
        if (visibility.worldAndCameraOverlay || visibility.actorText ||
            visibility.navigationCacheText)
        {
            drawDebugOverlay(
                overlay,
                viewport,
                visibility.worldAndCameraOverlay,
                visibility.actorText,
                visibility.navigationCacheText,
                visibility.stateMachine);
        }
        if (visibility.stateMachine)
        {
            drawMachineGraph(tools.machineEditors, overlay.machine, tools.machineActor.has_value());
        }
        tools.frameHistory.push(profile);
        drawFrameProfile(
            tools.frameHistory,
            tools.frameSelection,
            tools.frameAxes,
            visibility.frameProfileDetails);
    }
}
