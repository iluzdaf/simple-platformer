#pragma once

#include <cstddef>
#include <optional>

namespace simple_platformer
{
    class Game;
    struct TextureView;
    struct WindowViewport;

    // What the player asked for through the interface. The loop applies them after the
    // interface is built, so building it never changes the game.
    struct InterfaceRequests
    {
        bool toggleInventory = false;
        // The consumable clicked in the inventory.
        std::optional<std::size_t> useInventorySlot;
    };

    // The player-facing interface over the scene, in a fixed order: the health HUD, the
    // inventory bag, the locked exit hint, the completion message, the pause notice
    // while the simulation is paused, and the inventory while it is open. With no
    // viewport there is nothing to draw against. Build it before the simulation so a
    // bag click pauses the same frame instead of firing a shot.
    InterfaceRequests drawInterface(
        const Game& game,
        const TextureView& atlas,
        const std::optional<WindowViewport>& viewport,
        bool inventoryOpen,
        bool simulationPaused);
}
