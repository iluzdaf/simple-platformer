#pragma once

#include <optional>

namespace simple_platformer
{
    struct DebugOverlay;
    struct WindowViewport;

    // Draws all world and text layers from the game's snapshot.
    // With no viewport only the text is drawn.
    void drawDebugTools(const DebugOverlay& overlay, const std::optional<WindowViewport>& viewport);
}
