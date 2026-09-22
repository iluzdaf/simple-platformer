#pragma once

#include <optional>

#include "graphics/display_viewport.hpp"

namespace simple_platformer
{
    struct DebugOverlay;
    class FrameHistory;

    void drawDebugOverlay(const DebugOverlay& scene, const std::optional<WindowViewport>& viewport);

    // The frame-time plot and its breakdown, in a small panel under the HUD.
    void drawFrameProfile(const FrameHistory& history);
}
