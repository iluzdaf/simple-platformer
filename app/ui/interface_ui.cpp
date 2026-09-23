#include "interface_ui.hpp"

#include <cstddef>
#include <optional>

#include "game/game.hpp"
#include "graphics/display_viewport.hpp"
#include "ui/completion_ui.hpp"
#include "ui/exit_hint_ui.hpp"
#include "ui/health_hud_ui.hpp"
#include "ui/inventory_ui.hpp"

namespace simple_platformer
{
    std::optional<std::size_t> drawInterface(
        const Game& game,
        const TextureView& atlas,
        const std::optional<WindowViewport>& viewport,
        bool inventoryOpen)
    {
        if (!viewport.has_value())
        {
            return std::nullopt;
        }
        drawHealthHud(game.playerHealth(), atlas, *viewport);
        if (!game.complete())
        {
            drawLockedExitHint(game, atlas, *viewport);
        }
        drawLevelCompletion(game, *viewport);
        if (inventoryOpen && !game.complete())
        {
            return drawInventory(game, atlas, *viewport);
        }
        return std::nullopt;
    }
}
