#include "health_hud_ui.hpp"

#include <stdexcept>

#include <imgui.h>

#include "graphics/display_viewport.hpp"
#include "content/hud_catalog.hpp"
#include "graphics/sprite_renderer.hpp"
#include "simple_platformer/actor/actor.hpp"
#include "ui/hud_draw.hpp"
#include "ui/hud_layout.hpp"

namespace simple_platformer
{
    void drawHealthHud(
        const Health& health,
        const HudIcons& icons,
        const TextureView& atlas,
        const WindowViewport& viewport)
    {
        if (!atlasContains(atlas, icons.fullHeart) || !atlasContains(atlas, icons.emptyHeart))
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
            drawAtlasRegion(
                *drawList,
                atlas,
                heart < health.current ? icons.fullHeart : icons.emptyHeart,
                position,
                {position.x + size.x, position.y + size.y});
            position.x += (HudIconSize + HudGap) * viewport.scale.x;
        }
    }
}
