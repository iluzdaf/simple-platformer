#pragma once

namespace simple_platformer
{
    struct Health;
    struct HudIcons;
    struct Texture;
    struct WindowViewport;

    void drawHealthHud(
        const Health& health,
        const HudIcons& icons,
        const Texture& atlas,
        const WindowViewport& viewport);
}
