#pragma once

#include <imgui.h>

namespace simple_platformer
{
    struct SpriteRegion;
    struct TextureView;

    constexpr ImU32 HudTextColour = IM_COL32(255, 255, 255, 255);

    // Text over a one-pixel dark shadow, so it reads on any part of the scene.
    void drawShadowedText(ImDrawList& drawList, ImVec2 position, ImU32 colour, const char* text);
    // Shadowed text centred on the x position, its top at y.
    void drawCenteredText(ImDrawList& drawList, float centerX, float y, const char* text);

    // The atlas region stretched over the screen rectangle.
    void drawAtlasRegion(
        ImDrawList& drawList,
        const TextureView& atlas,
        const SpriteRegion& region,
        ImVec2 topLeft,
        ImVec2 bottomRight);
}
