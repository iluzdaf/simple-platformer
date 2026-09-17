#pragma once

#include <cstddef>
#include <optional>

namespace simple_platformer
{
    class ExampleGame;
    struct TextureView;
    struct WindowViewport;

    // Clicking a consumable returns its slot. The game applies the request after UI construction.
    std::optional<std::size_t> drawInventory(
        const ExampleGame& game,
        const TextureView& atlas,
        const WindowViewport& viewport);
}
