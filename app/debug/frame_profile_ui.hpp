#pragma once

namespace simple_platformer
{
    class FrameAxes;
    class FrameHistory;
    class FrameSelection;

    // The frame-time plot and its breakdown, in a panel at the top-left corner. Like the
    // rest of the overlay it lets clicks through to the game; only its legend and its
    // plot take them. A press on the plot picks the frame under the cursor and holding it
    // scrubs: the panel then shows the history as it was and the picked frame's own
    // costs, until the picked frame is clicked again. The axes grow at once to the worst
    // live frame and come down a full history after it has left.
    void drawFrameProfile(const FrameHistory& live, FrameSelection& selection, FrameAxes& axes);
}
