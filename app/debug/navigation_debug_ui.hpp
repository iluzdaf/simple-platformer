#pragma once

#include <imgui.h>

namespace simple_platformer
{
    struct Aabb;
    struct NavigationCacheDebugInfo;
    struct WindowViewport;

    // The connection cache's cells by state, and for the cell under the cursor its
    // footprint, its connections and the cells reachable from it.
    void drawNavigationCache(
        ImDrawList& drawList,
        const NavigationCacheDebugInfo& cache,
        const Aabb& cameraBounds,
        const WindowViewport& viewport);
}
