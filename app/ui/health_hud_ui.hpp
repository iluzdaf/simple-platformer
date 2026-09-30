#pragma once

namespace simple_platformer
{
    struct Health;
    struct HudIcons;
    struct TextureView;
    struct WindowViewport;

    void drawHealthHud(
        const Health& health,
        const HudIcons& icons,
        const TextureView& atlas,
        const WindowViewport& viewport);
}
