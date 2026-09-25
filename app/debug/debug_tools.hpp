#pragma once

#include <cstddef>
#include <optional>

#include <glm/vec2.hpp>

#include "debug/frame_axes.hpp"
#include "debug/frame_selection.hpp"
#include "debug/machine_graph_ui.hpp"
#include "simple_platformer/timing/frame_profile.hpp"

namespace simple_platformer
{
    class Game;
    struct WindowViewport;

    // What the debug tools keep between frames: the frame history, which the loop
    // pushes every frame so the panel is warm when the overlay opens; the picked frame
    // and the axes' tops of the frame panel; and the machine window's editors.
    struct DebugTools
    {
        FrameHistory frameHistory;
        FrameSelection frameSelection;
        FrameAxes frameAxes;
        MachineGraph machineGraph;
    };

    // The debug tools over the scene while the overlay is open, in a fixed order: the
    // world and text overlay, the machine window, and the frame panel. It is the short
    // list of what the overlay draws, as drawInterface is for what the player sees. The
    // cursor, in internal pixels, and the body index pick what the overlay is asked to
    // show; with no viewport only the text is drawn.
    void drawDebugTools(
        DebugTools& tools,
        const Game& game,
        float atlasWidth,
        std::optional<glm::vec2> internalCursor,
        std::size_t navigationBodyIndex,
        const std::optional<WindowViewport>& viewport);
}
