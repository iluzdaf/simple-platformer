#pragma once

namespace simple_platformer
{
    class FrameHistory;

    // The frame-time plot and its breakdown, in a panel at the top-left corner. Like the
    // rest of the overlay it lets clicks through to the game; only its legend takes them.
    void drawFrameProfile(const FrameHistory& history);
}
