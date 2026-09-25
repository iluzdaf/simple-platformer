#include "completion_ui.hpp"

#include <optional>

#include <glm/vec2.hpp>
#include <imgui.h>

#include "game/game.hpp"
#include "graphics/display_viewport.hpp"
#include "ui/hud_draw.hpp"
#include "ui/hud_layout.hpp"

namespace simple_platformer
{
    void drawLevelCompletion(const Game& game, const WindowViewport& viewport)
    {
        if (!game.complete())
        {
            return;
        }
        const std::optional<glm::vec2> exitPosition = game.levelExitScreenPosition();
        if (!exitPosition.has_value())
        {
            return;
        }

        const ImVec2 doorTopCenter = {
            viewport.topLeft.x + exitPosition->x * viewport.scale.x,
            viewport.topLeft.y + exitPosition->y * viewport.scale.y};
        const float lineHeight = ImGui::GetTextLineHeight();
        const float firstLineY =
            doorTopCenter.y - lineHeight * 3.0F - GapAboveDoor * viewport.scale.y;
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        drawCenteredText(*drawList, doorTopCenter.x, firstLineY, "Completed");
        drawCenteredText(*drawList, doorTopCenter.x, firstLineY + lineHeight, "Press R to");
        drawCenteredText(*drawList, doorTopCenter.x, firstLineY + lineHeight * 2.0F, "restart");
    }
}
