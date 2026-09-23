#include "completion_ui.hpp"

#include <optional>

#include <glm/vec2.hpp>
#include <imgui.h>

#include "game/game.hpp"
#include "graphics/display_viewport.hpp"
#include "ui/hud_draw.hpp"

namespace simple_platformer
{
    namespace
    {
        constexpr ImU32 TextColour = IM_COL32(255, 255, 255, 255);
        // The message ends this far above the door, in window pixels.
        constexpr float GapAboveDoor = 4.0F;

        void drawCenteredText(ImDrawList& drawList, ImVec2 center, float y, const char* text)
        {
            const float left = center.x - ImGui::CalcTextSize(text).x * 0.5F;
            drawShadowedText(drawList, {left, y}, TextColour, text);
        }
    }

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
        const float firstLineY = doorTopCenter.y - lineHeight * 3.0F - GapAboveDoor;
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        drawCenteredText(*drawList, doorTopCenter, firstLineY, "Completed");
        drawCenteredText(*drawList, doorTopCenter, firstLineY + lineHeight, "Press R to");
        drawCenteredText(*drawList, doorTopCenter, firstLineY + lineHeight * 2.0F, "restart");
    }
}
