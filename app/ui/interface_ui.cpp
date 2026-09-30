#include "interface_ui.hpp"

#include <optional>

#include "game/game.hpp"
#include "graphics/display_viewport.hpp"
#include "ui/completion_ui.hpp"
#include "ui/exit_hint_ui.hpp"
#include "ui/health_hud_ui.hpp"
#include "ui/inventory_ui.hpp"
#include "ui/pause_ui.hpp"

namespace simple_platformer
{
    InterfaceRequests drawInterface(
        const Game& game,
        const Texture& atlas,
        const std::optional<WindowViewport>& viewport,
        bool inventoryOpen,
        bool simulationPaused)
    {
        InterfaceRequests requests;
        if (!viewport.has_value())
        {
            return requests;
        }
        drawHealthHud(game.playerHealth(), game.hudIcons(), atlas, *viewport);
        if (!game.complete())
        {
            requests.toggleInventory = drawInventoryButton(atlas, game.hudIcons().bag, *viewport);
            drawLockedExitHint(game, atlas, *viewport);
        }
        drawLevelCompletion(game, *viewport);
        if (simulationPaused && !game.complete())
        {
            drawPauseNotice(*viewport);
        }
        if (inventoryOpen && !game.complete())
        {
            requests.useInventorySlot = drawInventory(game, atlas, *viewport);
        }
        return requests;
    }
}
