#include "debug_tools.hpp"

#include "debug_overlay.hpp"
#include "debug_overlay_ui.hpp"
#include "frame_profile_ui.hpp"

#include <optional>

namespace simple_platformer
{
    void drawDebugTools(
        DebugTools& tools,
        const DebugOverlay& overlay,
        const std::optional<WindowViewport>& viewport)
    {
        drawDebugOverlay(overlay, viewport);
        tools.machineGraph.draw(overlay.machine);
        drawFrameProfile(tools.frameHistory, tools.frameSelection, tools.frameAxes);
    }
}
