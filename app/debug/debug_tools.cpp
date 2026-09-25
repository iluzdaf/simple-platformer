#include "debug_tools.hpp"

#include "debug_overlay.hpp"
#include "debug_overlay_ui.hpp"
#include "frame_profile_ui.hpp"
#include "game/game.hpp"

#include <cstddef>
#include <optional>

#include <glm/vec2.hpp>

namespace simple_platformer
{
    void drawDebugTools(
        DebugTools& tools,
        const Game& game,
        float atlasWidth,
        std::optional<glm::vec2> internalCursor,
        std::size_t navigationBodyIndex,
        const std::optional<WindowViewport>& viewport)
    {
        const DebugOverlay overlay =
            game.debugOverlay(atlasWidth, internalCursor, navigationBodyIndex);
        drawDebugOverlay(overlay, viewport);
        tools.machineGraph.draw(overlay.machine);
        drawFrameProfile(tools.frameHistory, tools.frameSelection, tools.frameAxes);
    }
}
