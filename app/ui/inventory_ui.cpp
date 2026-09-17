#include "inventory_ui.hpp"

#include <cstddef>
#include <cstdio>
#include <optional>

#include <imgui.h>

#include "game/example_game.hpp"
#include "graphics/display_viewport.hpp"
#include "graphics/sprite_renderer.hpp"
#include "simple_platformer/inventory/inventory.hpp"
#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "ui/hud_layout.hpp"
#include "ui/inventory_layout.hpp"

namespace simple_platformer
{
    std::optional<std::size_t> drawInventory(
        const ExampleGame& game,
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
                    const SpriteRegion& region = item.icon.region;
                    const float width = static_cast<float>(atlas.width);
                    const float height = static_cast<float>(atlas.height);
                    const ImVec2 iconMinimum = {
                        slotMinimum.x + iconPadding, slotMinimum.y + iconPadding};
                    const ImVec2 iconMaximum = {
                        slotMaximum.x - iconPadding, slotMaximum.y - iconPadding};
                    drawList->AddImage(
                        static_cast<ImTextureID>(atlas.handle),
                        iconMinimum,
                        iconMaximum,
                        {region.position.x / width, region.position.y / height},
                        {(region.position.x + region.size.x) / width,
                         (region.position.y + region.size.y) / height});

                    char count[16];
                    std::snprintf(count, sizeof(count), "%d", slot->quantity);
                    drawList->AddText(
                        {iconMinimum.x + 1.0F, iconMinimum.y + 1.0F},
                        IM_COL32(0, 0, 0, 220),
                        count);
                    drawList->AddText(iconMinimum, IM_COL32(255, 255, 255, 255), count);
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
