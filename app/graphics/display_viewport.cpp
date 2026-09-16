#include "graphics/display_viewport.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"

namespace simple_platformer
{
    std::optional<DisplayViewport> makeDisplayViewport(glm::ivec2 framebufferSize)
    {
        if (framebufferSize.x <= 0 || framebufferSize.y <= 0)
        {
            return std::nullopt;
        }

        const int scale =
            std::min(framebufferSize.x / InternalWidth, framebufferSize.y / InternalHeight);
        if (scale <= 0)
        {
            return std::nullopt;
        }

        const glm::ivec2 size = {InternalWidth * scale, InternalHeight * scale};
        const glm::ivec2 lowerMargin = (framebufferSize - size) / 2;
        const int topMargin = framebufferSize.y - size.y - lowerMargin.y;
        return DisplayViewport{{lowerMargin.x, topMargin}, lowerMargin.y, size, scale};
    }

    std::optional<glm::vec2> windowToInternal(
        glm::vec2 windowPosition,
        glm::ivec2 windowSize,
        glm::ivec2 framebufferSize)
    {
        if (!std::isfinite(windowPosition.x) || !std::isfinite(windowPosition.y) ||
            windowSize.x <= 0 || windowSize.y <= 0)
        {
            return std::nullopt;
        }

        const std::optional<DisplayViewport> viewport = makeDisplayViewport(framebufferSize);
        if (!viewport.has_value())
        {
            return std::nullopt;
        }

        const glm::vec2 framebufferPosition = {
            windowPosition.x * static_cast<float>(framebufferSize.x) /
                static_cast<float>(windowSize.x),
            windowPosition.y * static_cast<float>(framebufferSize.y) /
                static_cast<float>(windowSize.y)};
        const glm::vec2 relative = framebufferPosition - glm::vec2(viewport->topLeftMargin);
        if (relative.x < 0.0F || relative.y < 0.0F ||
            relative.x >= static_cast<float>(viewport->size.x) ||
            relative.y >= static_cast<float>(viewport->size.y))
        {
            return std::nullopt;
        }

        return relative / static_cast<float>(viewport->scale);
    }
}
