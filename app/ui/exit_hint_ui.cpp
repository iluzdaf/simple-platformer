#include "exit_hint_ui.hpp"

#include <optional>

#include <glm/vec2.hpp>
#include <imgui.h>

#include "game/game.hpp"
#include "graphics/display_viewport.hpp"
#include "graphics/sprite_renderer.hpp"
#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    void drawLockedExitHint(
        const Game& game,
        const TextureView& atlas,
        const WindowViewport& viewport)
    {
        const std::optional<Sprite> icon = game.lockedExitHintIcon();
        const std::optional<glm::vec2> doorTopCenter = game.levelExitScreenPosition();
        if (!icon.has_value() || !doorTopCenter.has_value())
        {
            return;
        }

        constexpr float GapAboveDoor = 4.0F;
        const ImVec2 topLeft = {
            viewport.topLeft.x + (doorTopCenter->x - icon->size.x * 0.5F) * viewport.scale.x,
            viewport.topLeft.y +
                (doorTopCenter->y - GapAboveDoor - icon->size.y) * viewport.scale.y};
        const ImVec2 bottomRight = {
            topLeft.x + icon->size.x * viewport.scale.x,
            topLeft.y + icon->size.y * viewport.scale.y};
        const float atlasWidth = static_cast<float>(atlas.width);
        const float atlasHeight = static_cast<float>(atlas.height);
        const SpriteRegion& region = icon->region;
        ImGui::GetBackgroundDrawList()->AddImage(
            static_cast<ImTextureID>(atlas.handle),
            topLeft,
            bottomRight,
            {region.position.x / atlasWidth, region.position.y / atlasHeight},
            {(region.position.x + region.size.x) / atlasWidth,
             (region.position.y + region.size.y) / atlasHeight});
    }
}
