#include "debug_tools.hpp"

#include "debug/debug_overlay.hpp"
#include "debug_overlay_ui.hpp"

#include <optional>

namespace simple_platformer
{
    void drawDebugTools(const DebugOverlay& overlay, const std::optional<WindowViewport>& viewport)
    {
        drawDebugOverlay(overlay, viewport);
    }
}
