#pragma once

#include <optional>

#include "graphics/display_viewport.hpp"

namespace simple_platformer
{
    struct DebugOverlay;
    class FrameHistory;

    void drawDebugOverlay(const DebugOverlay& scene, const std::optional<WindowViewport>& viewport);

    // The frame-time plot and its breakdown, in a panel at the top-left corner. Returns
    // true while the pointer rests on the panel but not on a legend entry, the one thing
    // in it that takes a click, so the application can leave the mouse to the player.
    bool drawFrameProfile(const FrameHistory& history);
}
