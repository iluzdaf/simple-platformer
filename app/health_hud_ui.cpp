#include "health_hud_ui.hpp"

#include <stdexcept>

#include <imgui.h>

#include "graphics/display_viewport.hpp"
#include "graphics/sprite_renderer.hpp"
#include "simple_platformer/actor/actor.hpp"

namespace simple_platformer
{
    void drawHealthHud(
        const Health& health,
        const TextureView& atlas,
        const WindowViewport& viewport)
    {
        constexpr float HeartSize = 16.0F;
        constexpr float HeartTop = 192.0F;
        constexpr float FilledHeartLeft = 96.0F;
        constexpr float EmptyHeartLeft = 112.0F;
        constexpr float Margin = 4.0F;
        constexpr float Gap = 2.0F;

        if (atlas.width < static_cast<int>(EmptyHeartLeft + HeartSize) ||
            atlas.height < static_cast<int>(HeartTop + HeartSize))
        {
            throw std::invalid_argument("The health HUD atlas is missing its heart regions");
        }

        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        const ImTextureID texture = static_cast<ImTextureID>(atlas.handle);
        const float atlasWidth = static_cast<float>(atlas.width);
        const float atlasHeight = static_cast<float>(atlas.height);
        const ImVec2 size = {HeartSize * viewport.scale.x, HeartSize * viewport.scale.y};
        ImVec2 position = {
            viewport.topLeft.x + Margin * viewport.scale.x,
            viewport.topLeft.y + Margin * viewport.scale.y};

        for (int heart = 0; heart < health.maximum; ++heart)
        {
            const float sourceLeft = heart < health.current ? FilledHeartLeft : EmptyHeartLeft;
            const ImVec2 minimumUv = {sourceLeft / atlasWidth, HeartTop / atlasHeight};
            const ImVec2 maximumUv = {
                (sourceLeft + HeartSize) / atlasWidth, (HeartTop + HeartSize) / atlasHeight};
            drawList->AddImage(
                texture,
                position,
                {position.x + size.x, position.y + size.y},
                minimumUv,
                maximumUv);
            position.x += (HeartSize + Gap) * viewport.scale.x;
        }
    }
}
