#include "pause_ui.hpp"

#include <imgui.h>

#include "graphics/display_viewport.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "ui/hud_draw.hpp"
#include "ui/hud_layout.hpp"

namespace simple_platformer
{
    void drawPauseNotice(const WindowViewport& viewport)
    {
        const float centerX = viewport.topLeft.x + InternalViewportSize.x * viewport.scale.x * 0.5F;
        const float top = viewport.topLeft.y + HudMargin * viewport.scale.y;
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        const auto drawCentered = [&](float y, const char* text)
        {
            const float left = centerX - ImGui::CalcTextSize(text).x * 0.5F;
            drawShadowedText(*drawList, {left, y}, HudTextColour, text);
        };
        drawCentered(top, "Paused");
    }
}
