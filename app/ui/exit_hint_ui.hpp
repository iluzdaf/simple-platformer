#pragma once

namespace simple_platformer
{
    class Game;
    struct TextureView;
    struct WindowViewport;

    // Draws the icon of what a locked exit needs above it, while the game says to.
    void drawLockedExitHint(
        const Game& game,
        const TextureView& atlas,
        const WindowViewport& viewport);
}
