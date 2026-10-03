#pragma once

#include <imgui.h>

namespace simple_platformer
{
    struct Aabb;
    struct NavigationConnectionsDebugInfo;
    struct WindowViewport;

    // Draws connection counts for visible cells, then the cursor cell's footprint and
    // outgoing connections.
    void drawNavigationConnections(
        ImDrawList& drawList,
        const NavigationConnectionsDebugInfo& navigation,
        const Aabb& cameraBounds,
        const WindowViewport& viewport);

    // Draws the selected profile and connected-cell count for the displayed view,
    // then advances the text position.
    void drawNavigationTotals(
        ImDrawList& drawList,
        const NavigationConnectionsDebugInfo& navigation,
        ImVec2& position);
}
