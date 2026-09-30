#pragma once

#include <cstddef>
#include <optional>

namespace simple_platformer
{
    class Game;
    struct SpriteRegion;
    struct Texture;
    struct WindowViewport;

    // The bag icon in the HUD corner. Returns whether it was clicked this frame.
    bool drawInventoryButton(
        const Texture& atlas,
        const SpriteRegion& bagIcon,
        const WindowViewport& viewport);

    // Clicking a consumable returns its slot. The game applies the request after UI construction.
    std::optional<std::size_t> drawInventory(
        const Game& game,
        const Texture& atlas,
        const WindowViewport& viewport);
}
