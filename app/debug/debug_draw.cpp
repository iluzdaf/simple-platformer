#include "debug_draw.hpp"

#include <imgui.h>

#include <glm/vec2.hpp>

#include "graphics/display_viewport.hpp"
#include "simple_platformer/math/aabb.hpp"

namespace simple_platformer
{
    ImVec2 screenPosition(
        glm::vec2 worldPosition,
        const Aabb& cameraBounds,
        const WindowViewport& viewport)
    {
        return {
            viewport.topLeft.x + (worldPosition.x - cameraBounds.position.x) * viewport.scale.x,
            viewport.topLeft.y + (worldPosition.y - cameraBounds.position.y) * viewport.scale.y};
    }

    void drawWorldBounds(
        ImDrawList& drawList,
        const Aabb& bounds,
        const Aabb& cameraBounds,
        const WindowViewport& viewport,
        ImU32 colour)
    {
        const ImVec2 minimum = screenPosition(bounds.position, cameraBounds, viewport);
        const ImVec2 maximum = {
            minimum.x + bounds.size.x * viewport.scale.x,
            minimum.y + bounds.size.y * viewport.scale.y};
        drawList.AddRect(minimum, maximum, colour, 0.0F, 0, 2.0F);
    }

    void drawTextLine(
        ImDrawList& drawList,
        ImVec2& position,
        const char* text,
        ImU32 colour,
        float indentation)
    {
        drawList.AddText({position.x + indentation, position.y}, colour, text);
        position.y += ImGui::GetTextLineHeight();
    }
}
