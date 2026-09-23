#pragma once

namespace simple_platformer
{
    struct Health;
    struct TextureView;
    struct WindowViewport;

    void drawHealthHud(
        const Health& health,
        const TextureView& atlas,
        const WindowViewport& viewport);
}
