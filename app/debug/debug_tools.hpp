#pragma once

#include <optional>

namespace simple_platformer
{
    struct DebugOverlay;
    struct WindowViewport;

    // Independently visible parts of the debug tools.
    struct DebugToolVisibility
    {
        bool worldAndCameraOverlay = true;
        bool actorText = false;
        bool navigationConnectionsText = false;
    };

    // Draws the independently optional world and text layers from the game's snapshot.
    // With no viewport only the text is drawn.
    void drawDebugTools(
        const DebugOverlay& overlay,
        const std::optional<WindowViewport>& viewport,
        const DebugToolVisibility& visibility);
}
