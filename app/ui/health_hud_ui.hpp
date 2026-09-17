#pragma once

namespace simple_platformer
{
    struct Health;
    struct TextureView;
    struct WindowViewport;

    // Build before simulation so a bag click opens the inventory without firing a shot.
    bool drawInventoryButton(const TextureView& atlas, const WindowViewport& viewport);

    void drawHealthHud(
        const Health& health,
        const TextureView& atlas,
        const WindowViewport& viewport);
}
