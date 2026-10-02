#pragma once

#include <imgui.h>

namespace simple_platformer
{
    struct Aabb;
    struct NavigationConnectionsDebugInfo;
    struct WindowViewport;

    // The current map's connections, and for the cell under the cursor its
    // footprint and its connections.
    void drawNavigationConnections(
        ImDrawList& drawList,
        const NavigationConnectionsDebugInfo& navigation,
        const Aabb& cameraBounds,
        const WindowViewport& viewport);

    // Draws selected-profile and navigation-wide totals, then advances the text position.
    void drawNavigationTotals(
        ImDrawList& drawList,
        const NavigationConnectionsDebugInfo& navigation,
        ImVec2& position);
}
