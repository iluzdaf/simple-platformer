#pragma once

#include <cstddef>
#include <optional>

namespace simple_platformer
{
    class Game;
    struct TextureView;
    struct WindowViewport;

    // The bag icon in the HUD corner. Returns whether it was clicked this frame.
    bool drawInventoryButton(const TextureView& atlas, const WindowViewport& viewport);

    // Clicking a consumable returns its slot. The game applies the request after UI construction.
    std::optional<std::size_t> drawInventory(
        const Game& game,
        const TextureView& atlas,
        const WindowViewport& viewport);
}
