#include "frame_selection.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>

#include "simple_platformer/timing/frame_profile.hpp"

namespace simple_platformer
{
    std::optional<std::size_t> frameAtPlotFraction(
        float fraction,
        std::size_t capacity,
        std::size_t count)
    {
        if (count == 0 || !std::isfinite(fraction))
        {
            return std::nullopt;
        }
        const float x = fraction * static_cast<float>(capacity);
        if (x < -0.5F)
        {
            return std::nullopt;
        }
        const auto nearest = static_cast<std::size_t>(std::lround(std::fmax(x, 0.0F)));
        if (nearest >= count)
        {
            return std::nullopt;
        }
        return nearest;
    }

    std::size_t frameNearestPlotFraction(float fraction, std::size_t capacity, std::size_t count)
    {
        if (count == 0)
        {
            throw std::invalid_argument("No frame has been plotted to scrub to");
        }
        if (!std::isfinite(fraction))
        {
            return 0;
        }
        const float x = std::fmin(std::fmax(fraction, 0.0F), 1.0F) * static_cast<float>(capacity);
        return std::min(static_cast<std::size_t>(std::lround(x)), count - 1);
    }

    void FrameSelection::select(const FrameHistory& live, std::size_t index)
    {
        const FrameHistory& history = frozen.has_value() ? *frozen : live;
        if (index >= history.size())
        {
            throw std::out_of_range("No frame has been plotted at that index");
        }
        if (!frozen.has_value())
        {
            frozen = live;
        }
        selected = index;
    }

    void FrameSelection::clear()
    {
        frozen.reset();
        selected = 0;
    }

    std::optional<std::size_t> FrameSelection::selectedIndex() const
    {
        if (!frozen.has_value())
        {
            return std::nullopt;
        }
        return selected;
    }

    const FrameHistory* FrameSelection::kept() const
    {
        return frozen.has_value() ? &*frozen : nullptr;
    }

    const FrameProfile& FrameSelection::selectedFrame() const
    {
        if (!frozen.has_value())
        {
            throw std::logic_error("No frame is selected");
        }
        return frozen->frameOldestFirst(selected);
    }
}
