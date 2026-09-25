#pragma once

namespace simple_platformer
{
    class FrameAxes;
    class FrameHistory;
    class FrameSelection;

    // The frame-time plot at the top-left corner, with an optional legend and wheel-
    // scrollable breakdown filling the window below it. Like the rest of the overlay the
    // plot panel lets clicks through to the game; only the plot picker and the optional
    // lower panel take them. A press on the plot picks the frame under the cursor and
    // holding it scrubs: the breakdown then shows the history as it was and the picked
    // frame's own costs, until the picked frame is clicked again. The axes grow at once
    // to the worst live frame and come down a full history after it has left.
    void drawFrameProfile(
        const FrameHistory& live,
        FrameSelection& selection,
        FrameAxes& axes,
        bool showDetails);
}
