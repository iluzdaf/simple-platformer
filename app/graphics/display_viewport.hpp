#pragma once

#include <optional>

#include <glm/vec2.hpp>

namespace simple_platformer
{
    struct DisplayViewport
    {
        // Cursor positions use a top-left origin, while OpenGL uses a bottom-left origin.
        glm::ivec2 topLeftMargin = {0, 0};
        int bottomMargin = 0;
        glm::ivec2 size = {0, 0};
        int scale = 1;
    };

    std::optional<DisplayViewport> makeDisplayViewport(glm::ivec2 framebufferSize);

    // GLFW cursor coordinates use window points, which can differ from framebuffer pixels on
    // high-DPI displays. Positions in the black letterbox area return nullopt.
    std::optional<glm::vec2> windowToInternal(
        glm::vec2 windowPosition,
        glm::ivec2 windowSize,
        glm::ivec2 framebufferSize);
}
