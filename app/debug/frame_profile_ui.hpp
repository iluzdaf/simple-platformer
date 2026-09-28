#pragma once

namespace simple_platformer
{
    class FrameAxes;
    class FrameHistory;
    class FrameSelection;

    enum class FramePlotRequest
    {
        None,
        Pause,
        Resume
    };

    // The frame-time plot at the top-left corner, with an optional legend and wheel-
    // scrollable breakdown filling the window below it. Like the rest of the overlay the
    // plot panel lets clicks through to the game; only the plot picker and the optional
    // lower panel take them. A press on the plot requests a game pause and picks the
    // frame under the cursor. Holding it scrubs through the paused history; clicking
    // the picked frame again requests a resume. The axes grow at once to the worst
    // live frame and come down a full history after it leaves.
    FramePlotRequest drawFrameProfile(
        const FrameHistory& live,
        FrameSelection& selection,
        FrameAxes& axes,
        bool showDetails);
}
