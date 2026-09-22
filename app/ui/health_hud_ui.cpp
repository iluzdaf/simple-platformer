#include "health_hud_ui.hpp"

#include <stdexcept>

#include <imgui.h>

#include "graphics/display_viewport.hpp"
#include "graphics/sprite_renderer.hpp"
#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "ui/hud_layout.hpp"

namespace simple_platformer
{
    bool drawInventoryButton(const TextureView& atlas, const WindowViewport& viewport)
    {
        constexpr float BagLeft = 64.0F;
        constexpr float BagTop = 216.0F;
        if (atlas.width < static_cast<int>(BagLeft + HudIconSize) ||
            atlas.height < static_cast<int>(BagTop + HudIconSize))
        {
            throw std::invalid_argument("The HUD atlas is missing its bag region");
        }
        const ImVec2 size{HudIconSize * viewport.scale.x, HudIconSize * viewport.scale.y};
        const ImVec2 position{
            viewport.topLeft.x + HudMargin * viewport.scale.x,
            viewport.topLeft.y +
                (static_cast<float>(InternalHeight) - HudMargin - HudIconSize) * viewport.scale.y};
        ImGui::SetNextWindowPos(position, ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0F, 0.0F});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, {0.0F, 0.0F});
        bool clicked = false;
        if (ImGui::Begin(
                "Inventory bag##hud",
                nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                    ImGuiWindowFlags_NoNav))
        {
            ImGui::InvisibleButton("Open inventory", size);
            const bool hovered = ImGui::IsItemHovered();
            // Act on press, before gameplay consumes the same mouse-button edge.
            clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
            const float width = static_cast<float>(atlas.width);
            const float height = static_cast<float>(atlas.height);
            ImGui::GetWindowDrawList()->AddImage(
                static_cast<ImTextureID>(atlas.handle),
                position,
                {position.x + size.x, position.y + size.y},
                {BagLeft / width, BagTop / height},
                {(BagLeft + HudIconSize) / width, (BagTop + HudIconSize) / height});
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
        return clicked;
    }

    void drawHealthHud(
        const Health& health,
        const TextureView& atlas,
        const WindowViewport& viewport)
    {
        constexpr float HeartTop = 192.0F;
        constexpr float FilledHeartLeft = 96.0F;
        constexpr float EmptyHeartLeft = 112.0F;

        if (atlas.width < static_cast<int>(EmptyHeartLeft + HudIconSize) ||
            atlas.height < static_cast<int>(HeartTop + HudIconSize))
        {
            throw std::invalid_argument("The health HUD atlas is missing its heart regions");
        }

        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        const ImTextureID texture = static_cast<ImTextureID>(atlas.handle);
        const float atlasWidth = static_cast<float>(atlas.width);
        const float atlasHeight = static_cast<float>(atlas.height);
        const ImVec2 size = {HudIconSize * viewport.scale.x, HudIconSize * viewport.scale.y};
        ImVec2 position = {
            viewport.topLeft.x + HudMargin * viewport.scale.x,
            viewport.topLeft.y + HudMargin * viewport.scale.y};

        for (int heart = 0; heart < health.maximum; ++heart)
        {
            const float sourceLeft = heart < health.current ? FilledHeartLeft : EmptyHeartLeft;
            const ImVec2 minimumUv = {sourceLeft / atlasWidth, HeartTop / atlasHeight};
            const ImVec2 maximumUv = {
                (sourceLeft + HudIconSize) / atlasWidth, (HeartTop + HudIconSize) / atlasHeight};
            drawList->AddImage(
                texture,
                position,
                {position.x + size.x, position.y + size.y},
                minimumUv,
                maximumUv);
            position.x += (HudIconSize + HudGap) * viewport.scale.x;
        }
    }
}
