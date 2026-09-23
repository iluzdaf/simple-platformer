#pragma once

#include <cstddef>
#include <optional>

namespace simple_platformer
{
    class Game;
    struct TextureView;
    struct WindowViewport;

    // The player-facing interface over the scene, in a fixed order: the health HUD, the
    // locked exit hint, the completion message, and the inventory while it is open. With
    // no viewport there is nothing to draw against. Clicking a consumable in the
    // inventory returns its slot; the game applies the request after UI construction.
    std::optional<std::size_t> drawInterface(
        const Game& game,
        const TextureView& atlas,
        const std::optional<WindowViewport>& viewport,
        bool inventoryOpen);
}
