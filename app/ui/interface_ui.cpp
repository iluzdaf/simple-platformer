#include "interface_ui.hpp"

#include <cstddef>
#include <cstdio>
#include <optional>

#include <glm/vec2.hpp>
#include <imgui.h>

#include "content/hud_catalog.hpp"
#include "game/game.hpp"
#include "graphics/display_viewport.hpp"
#include "graphics/sprite_renderer.hpp"
#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/inventory/inventory.hpp"
#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "ui/hud_draw.hpp"
#include "ui/hud_layout.hpp"
#include "ui/inventory_layout.hpp"

namespace simple_platformer
{
    namespace
    {

        void drawHealthHud(
            const Health& health,
            const HudIcons& icons,
            const Texture& atlas,
            const WindowViewport& viewport)
        {
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

        bool drawInventoryButton(
            const Texture& atlas,
            const SpriteRegion& bagIcon,
            const WindowViewport& viewport)
        {
            const ImVec2 size{HudIconSize * viewport.scale.x, HudIconSize * viewport.scale.y};
            const ImVec2 position{
                viewport.topLeft.x + HudMargin * viewport.scale.x,
                viewport.topLeft.y +
                    (static_cast<float>(InternalHeight) - HudMargin - HudIconSize) *
                        viewport.scale.y};
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
                    bagIcon,
                    position,
                    {position.x + size.x, position.y + size.y});
            }
            ImGui::End();
            ImGui::PopStyleVar(2);
            return clicked;
        }

        void drawLockedExitHint(
            const Game& game,
            const Texture& atlas,
            const WindowViewport& viewport)
        {
            const std::optional<Sprite> icon = game.lockedExitHintIcon();
            const std::optional<glm::vec2> doorTopCenter = game.levelExitScreenPosition();
            if (!icon.has_value() || !doorTopCenter.has_value())
            {
                return;
            }

            const ImVec2 topLeft = {
                viewport.topLeft.x +
                    (doorTopCenter->x - icon->region.size.x * 0.5F) * viewport.scale.x,
                viewport.topLeft.y +
                    (doorTopCenter->y - GapAboveDoor - icon->region.size.y) * viewport.scale.y};
            const ImVec2 bottomRight = {
                topLeft.x + icon->region.size.x * viewport.scale.x,
                topLeft.y + icon->region.size.y * viewport.scale.y};
            drawAtlasRegion(
                *ImGui::GetBackgroundDrawList(), atlas, icon->region, topLeft, bottomRight);
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
            const float firstLineY =
                doorTopCenter.y - lineHeight * 3.0F - GapAboveDoor * viewport.scale.y;
            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            drawCenteredText(*drawList, doorTopCenter.x, firstLineY, "Completed");
            drawCenteredText(*drawList, doorTopCenter.x, firstLineY + lineHeight, "Press R to");
            drawCenteredText(*drawList, doorTopCenter.x, firstLineY + lineHeight * 2.0F, "restart");
        }

        // Inventory grid layout, in internal pixels before the viewport scales them.
        constexpr float SlotSize = 20.0F;
        constexpr float IconPadding = 2.0F;
        constexpr float GridPadding = 2.0F;

        constexpr ImU32 SlotColour = IM_COL32(40, 44, 52, 220);
        constexpr ImU32 HoveredSlotColour = IM_COL32(72, 76, 84, 240);
        constexpr ImU32 SlotBorderColour = IM_COL32(150, 156, 168, 220);

        // Draws one slot and returns whether its consumable was clicked.
        bool drawInventorySlot(
            const Game& game,
            const Texture& atlas,
            const std::optional<ItemStack>& slot,
            float slotSize,
            float iconPadding)
        {
            const bool clicked = ImGui::InvisibleButton("slot", {slotSize, slotSize});
            const ImVec2 slotMinimum = ImGui::GetItemRectMin();
            const ImVec2 slotMaximum = ImGui::GetItemRectMax();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImU32 background = ImGui::IsItemHovered() ? HoveredSlotColour : SlotColour;
            drawList->AddRectFilled(slotMinimum, slotMaximum, background);
            drawList->AddRect(slotMinimum, slotMaximum, SlotBorderColour, 0.0F, 0, 1.0F);

            if (!slot.has_value())
            {
                return false;
            }
            const ItemDefinition& item = game.itemDefinition(slot->item);
            const ImVec2 iconMinimum = {slotMinimum.x + iconPadding, slotMinimum.y + iconPadding};
            const ImVec2 iconMaximum = {slotMaximum.x - iconPadding, slotMaximum.y - iconPadding};
            drawAtlasRegion(*drawList, atlas, item.icon.region, iconMinimum, iconMaximum);

            char count[16];
            std::snprintf(count, sizeof(count), "%d", slot->quantity);
            drawShadowedText(*drawList, iconMinimum, HudTextColour, count);
            return clicked && item.effect != ItemEffect::None;
        }

        std::optional<std::size_t> drawInventory(
            const Game& game,
            const Texture& atlas,
            const WindowViewport& viewport)
        {
            const auto& slots = game.playerInventory().slots();
            const InventoryGridLayout layout = makeInventoryGridLayout(slots.size());
            const float slotSize = SlotSize * viewport.scale.x;
            const float iconPadding = IconPadding * viewport.scale.x;
            const float windowPadding = GridPadding * viewport.scale.x;
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
                    if (drawInventorySlot(game, atlas, slots[index], slotSize, iconPadding))
                    {
                        slotToUse = index;
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

    InterfaceRequests drawInterface(
        const Game& game,
        const Texture& atlas,
        const std::optional<WindowViewport>& viewport,
        bool inventoryOpen)
    {
        InterfaceRequests requests;
        if (!viewport.has_value())
        {
            return requests;
        }
        drawHealthHud(game.playerHealth(), game.hudIcons(), atlas, *viewport);
        if (!game.complete())
        {
            requests.toggleInventory = drawInventoryButton(atlas, game.hudIcons().bag, *viewport);
            drawLockedExitHint(game, atlas, *viewport);
        }
        drawLevelCompletion(game, *viewport);
        if (inventoryOpen && !game.complete())
        {
            requests.useInventorySlot = drawInventory(game, atlas, *viewport);
        }
        return requests;
    }
}
