#pragma once

struct GLFWwindow;

namespace simple_platformer
{
    struct ActorDebugScene;

    void drawActorDebugUi(
        const ActorDebugScene& scene,
        GLFWwindow* window,
        int framebufferWidth,
        int framebufferHeight);
}
