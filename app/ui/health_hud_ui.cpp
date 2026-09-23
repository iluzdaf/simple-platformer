#include "health_hud_ui.hpp"

#include <stdexcept>

#include <imgui.h>

#include "graphics/display_viewport.hpp"
#include "graphics/sprite_renderer.hpp"
#include "simple_platformer/actor/actor.hpp"
#include "ui/hud_draw.hpp"
#include "ui/hud_layout.hpp"

namespace simple_platformer
{
    namespace
    {
        // Where the heart icons sit in the atlas, in pixels.
        constexpr float HeartTop = 192.0F;
        constexpr float FilledHeartLeft = 96.0F;
        constexpr float EmptyHeartLeft = 112.0F;
    }

    void drawHealthHud(
        const Health& health,
        const TextureView& atlas,
        const WindowViewport& viewport)
    {
        if (atlas.width < static_cast<int>(EmptyHeartLeft + HudIconSize) ||
            atlas.height < static_cast<int>(HeartTop + HudIconSize))
        {
            throw std::invalid_argument("The health HUD atlas is missing its heart regions");
        }

        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        const ImVec2 size = {HudIconSize * viewport.scale.x, HudIconSize * viewport.scale.y};
        ImVec2 position = {
            viewport.topLeft.x + HudMargin * viewport.scale.x,
            viewport.topLeft.y + HudMargin * viewport.scale.y};

        for (int heart = 0; heart < health.maximum; ++heart)
        {
            const float sourceLeft = heart < health.current ? FilledHeartLeft : EmptyHeartLeft;
            drawAtlasRegion(
                *drawList,
                atlas,
                {{sourceLeft, HeartTop}, {HudIconSize, HudIconSize}},
                position,
                {position.x + size.x, position.y + size.y});
            position.x += (HudIconSize + HudGap) * viewport.scale.x;
        }
    }
}
