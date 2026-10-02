#include "debug_tools.hpp"

#include "debug_overlay.hpp"
#include "debug_overlay_ui.hpp"

#include <optional>

namespace simple_platformer
{
    void drawDebugTools(
        const DebugOverlay& overlay,
        const std::optional<WindowViewport>& viewport,
        const DebugToolVisibility& visibility)
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
    }
}
