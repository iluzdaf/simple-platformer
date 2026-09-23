#pragma once

#include <optional>

#include "graphics/display_viewport.hpp"

namespace simple_platformer
{
    struct DebugOverlay;
    class FrameHistory;

    void drawDebugOverlay(const DebugOverlay& scene, const std::optional<WindowViewport>& viewport);

    // The frame-time plot and its breakdown, in a panel at the top-left corner. Like the
    // rest of the overlay it lets clicks through to the game; only its legend takes them.
    void drawFrameProfile(const FrameHistory& history);
}
