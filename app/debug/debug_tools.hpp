#pragma once

#include <optional>

#include "debug/frame_axes.hpp"
#include "debug/frame_selection.hpp"
#include "debug/machine_graph_ui.hpp"
#include "simple_platformer/timing/frame_profile.hpp"

namespace simple_platformer
{
    struct DebugOverlay;
    struct WindowViewport;

    // Independently visible parts of the debug tools. They start collapsed so opening
    // the tools adds only the frame plot until the reader asks for another layer.
    struct DebugToolVisibility
    {
        bool frameProfileDetails = false;
        bool worldAndCameraOverlay = false;
        bool actorText = false;
        bool navigationCacheText = false;
        bool stateMachine = false;
    };

    // What the debug tools keep between frames: the frame history, the picked frame
    // and axes, and the machine window's editors and optional actor lock.
    struct DebugTools
    {
        FrameHistory frameHistory;
        FrameSelection frameSelection;
        FrameAxes frameAxes;
        MachineGraphEditors machineEditors;
        std::optional<ActorId> machineActor;
    };

    // The debug tools over the scene while the overlay is open, in a fixed order: the
    // independently optional world, text and machine layers, then the frame panel, which
    // first records the frame's profile, so the history holds only frames the overlay saw.
    // It is the short list of what the overlay draws, as drawInterface is for what the
    // player sees. It draws the overlay the game built and touches nothing else of the
    // game; with no viewport only the text is drawn.
    void drawDebugTools(
        DebugTools& tools,
        const FrameProfile& profile,
        const DebugOverlay& overlay,
        const std::optional<WindowViewport>& viewport,
        const DebugToolVisibility& visibility);
}
