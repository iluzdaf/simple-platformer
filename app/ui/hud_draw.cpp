#include "hud_draw.hpp"

#include <imgui.h>

#include "graphics/sprite_renderer.hpp"
#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    namespace
    {
        // The text shadow's colour and how far down and right it falls, in window pixels.
        constexpr ImU32 ShadowColour = IM_COL32(0, 0, 0, 220);
        constexpr float ShadowOffset = 1.0F;
    }

    void drawShadowedText(ImDrawList& drawList, ImVec2 position, ImU32 colour, const char* text)
    {
        drawList.AddText(
            {position.x + ShadowOffset, position.y + ShadowOffset}, ShadowColour, text);
        drawList.AddText(position, colour, text);
    }

    void drawAtlasRegion(
        ImDrawList& drawList,
        const TextureView& atlas,
        const SpriteRegion& region,
        ImVec2 topLeft,
        ImVec2 bottomRight)
    {
        const float width = static_cast<float>(atlas.width);
        const float height = static_cast<float>(atlas.height);
        drawList.AddImage(
            static_cast<ImTextureID>(atlas.handle),
            topLeft,
            bottomRight,
            {region.position.x / width, region.position.y / height},
            {(region.position.x + region.size.x) / width,
             (region.position.y + region.size.y) / height});
    }
}
