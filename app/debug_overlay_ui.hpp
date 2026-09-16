#pragma once

struct GLFWwindow;

namespace simple_platformer
{
    struct DebugOverlay;

    void drawDebugOverlay(
        const DebugOverlay& scene,
        GLFWwindow* window,
        int framebufferWidth,
        int framebufferHeight);
}
