#pragma once

namespace simple_platformer
{
    class Game;
    struct Texture;
    struct WindowViewport;

    // Draws the icon of what a locked exit needs above it, while the game says to.
    void drawLockedExitHint(const Game& game, const Texture& atlas, const WindowViewport& viewport);
}
