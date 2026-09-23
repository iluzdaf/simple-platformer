#pragma once

#include <cstddef>
#include <optional>

#include "simple_platformer/timing/frame_profile.hpp"

namespace simple_platformer
{
    // Which frame a click this far across the plot picks, or nothing where no frame has
    // been plotted. Frames sit at whole x positions, oldest first, on an axis as long as
    // the history's capacity, so the click goes to the nearest one.
    std::optional<std::size_t> frameAtPlotFraction(
        float fraction,
        std::size_t capacity,
        std::size_t count);

    // A frame picked from the plot to read at leisure. Picking one keeps a copy of the
    // history as it was, so the panel stops following the live frames and the picked
    // frame holds its place in the plot, until the pick is cleared.
    class FrameSelection
    {
    public:
        // Picks the frame at this index of the history shown; picking the selected frame
        // again clears the selection.
        void pick(const FrameHistory& live, std::size_t index);
        void clear();

        std::optional<std::size_t> selectedIndex() const;
        // The copy of the history kept while a frame is selected, to show in place of the
        // live one; nothing while live.
        const FrameHistory* kept() const;
        // Requires a selection.
        const FrameProfile& selectedFrame() const;

    private:
        std::optional<FrameHistory> frozen;
        std::size_t selected = 0;
    };
}
