#pragma once

namespace simple_platformer
{
    class Game;
    struct Texture;
    struct WindowViewport;

    // Draws the required item's icon above the exit while its locked-touch hint is active.
    void drawLockedExitHint(const Game& game, const Texture& atlas, const WindowViewport& viewport);
}
