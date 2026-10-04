#pragma once

#include <cstddef>
#include <optional>

namespace simple_platformer
{
    class Game;
    struct Texture;
    struct WindowViewport;

    // Requests collected while drawing the UI. The loop applies them afterward, so UI
    // construction does not change game state.
    struct InterfaceRequests
    {
        bool toggleInventory = false;
        // The consumable clicked in the inventory.
        std::optional<std::size_t> useInventorySlot;
    };

    // Draws the player interface and returns requests. No viewport means nothing is drawn
    // or requested. Call before simulation so opening the bag pauses that frame and the
    // same mouse press cannot also fire a shot.
    InterfaceRequests drawInterface(
        const Game& game,
        const Texture& atlas,
        const std::optional<WindowViewport>& viewport,
        bool inventoryOpen);
}
