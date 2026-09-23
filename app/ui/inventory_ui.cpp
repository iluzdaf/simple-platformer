#include "inventory_ui.hpp"

#include <cstddef>
#include <cstdio>
#include <optional>
#include <stdexcept>

#include <imgui.h>

#include "game/game.hpp"
#include "graphics/display_viewport.hpp"
#include "graphics/sprite_renderer.hpp"
#include "simple_platformer/inventory/inventory.hpp"
#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "ui/hud_draw.hpp"
#include "ui/hud_layout.hpp"
#include "ui/inventory_layout.hpp"

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
            drawAtlasRegion(
                *ImGui::GetWindowDrawList(),
                atlas,
                {{BagLeft, BagTop}, {HudIconSize, HudIconSize}},
                position,
                {position.x + size.x, position.y + size.y});
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
        return clicked;
    }

    std::optional<std::size_t> drawInventory(
        const Game& game,
        const TextureView& atlas,
        const WindowViewport& viewport)
    {
        const auto& slots = game.playerInventory().slots();
        const InventoryGridLayout layout = makeInventoryGridLayout(slots.size());
        const float slotSize = 20.0F * viewport.scale.x;
        const float iconPadding = 2.0F * viewport.scale.x;
        const float windowPadding = 2.0F * viewport.scale.x;
        const ImVec2 windowSize = {
            slotSize * static_cast<float>(layout.columns) +
                windowPadding * static_cast<float>(layout.columns + 1),
            slotSize * static_cast<float>(layout.rows) +
                windowPadding * static_cast<float>(layout.rows + 1)};
        const ImVec2 inventoryBottomLeft = {
            viewport.topLeft.x + HudMargin * viewport.scale.x,
            viewport.topLeft.y +
                (static_cast<float>(InternalHeight) - HudMargin - HudIconSize - HudGap) *
                    viewport.scale.y};

        std::optional<std::size_t> slotToUse;
        ImGui::SetNextWindowPos(inventoryBottomLeft, ImGuiCond_Always, {0.0F, 1.0F});
        ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {windowPadding, windowPadding});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {windowPadding, windowPadding});
        if (ImGui::Begin(
                "Inventory##grid",
                nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoSavedSettings))
        {
            for (std::size_t index = 0; index < slots.size(); ++index)
            {
                ImGui::PushID(static_cast<int>(index));
                const bool clicked = ImGui::InvisibleButton("slot", {slotSize, slotSize});
                const ImVec2 slotMinimum = ImGui::GetItemRectMin();
                const ImVec2 slotMaximum = ImGui::GetItemRectMax();
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const ImU32 background =
                    ImGui::IsItemHovered() ? IM_COL32(72, 76, 84, 240) : IM_COL32(40, 44, 52, 220);
                drawList->AddRectFilled(slotMinimum, slotMaximum, background);
                drawList->AddRect(
                    slotMinimum, slotMaximum, IM_COL32(150, 156, 168, 220), 0.0F, 0, 1.0F);

                const auto& slot = slots[index];
                if (slot.has_value())
                {
                    const ItemDefinition& item = game.itemDefinition(slot->item);
                    const ImVec2 iconMinimum = {
                        slotMinimum.x + iconPadding, slotMinimum.y + iconPadding};
                    const ImVec2 iconMaximum = {
                        slotMaximum.x - iconPadding, slotMaximum.y - iconPadding};
                    drawAtlasRegion(*drawList, atlas, item.icon.region, iconMinimum, iconMaximum);

                    char count[16];
                    std::snprintf(count, sizeof(count), "%d", slot->quantity);
                    drawShadowedText(*drawList, iconMinimum, IM_COL32(255, 255, 255, 255), count);
                    if (clicked && item.effect != ItemEffect::None)
                    {
                        slotToUse = index;
                    }
                }
                if ((index + 1) % layout.columns != 0 && index + 1 < slots.size())
                {
                    ImGui::SameLine();
                }
                ImGui::PopID();
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
        return slotToUse;
    }

}
