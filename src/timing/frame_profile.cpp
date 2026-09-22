#include "simple_platformer/timing/frame_profile.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace simple_platformer
{
    namespace
    {
        void requireSeconds(float seconds, const char* what)
        {
            if (!std::isfinite(seconds) || seconds < 0.0F)
            {
                throw std::invalid_argument(
                    std::string(what) + " must be a finite, non-negative number of seconds");
            }
        }
    }

    FrameHistory::FrameHistory(std::size_t capacity)
        : frames(capacity)
    {
        if (capacity == 0)
        {
            throw std::invalid_argument("A frame history needs room for at least one frame");
        }
    }

    void FrameHistory::push(const FrameProfile& frame)
    {
        requireSeconds(frame.frameSeconds, "Frame time");
        requireSeconds(frame.simulationSeconds, "Simulation time");
        requireSeconds(frame.sceneSeconds, "Scene time");
        requireSeconds(frame.renderSeconds, "Render time");
        requireSeconds(frame.interfaceSeconds, "Interface time");
        if (frame.simulationTicks < 0)
        {
            throw std::invalid_argument("A frame cannot run a negative number of ticks");
        }
        frames[next] = frame;
        next = (next + 1) % frames.size();
        count = std::min(count + 1, frames.size());
    }

    std::size_t FrameHistory::size() const
    {
        return count;
    }

    std::size_t FrameHistory::capacity() const
    {
        return frames.size();
    }

    const FrameProfile& FrameHistory::latest() const
    {
        if (count == 0)
        {
            throw std::logic_error("No frame has been recorded");
        }
        return frames[(next + frames.size() - 1) % frames.size()];
    }

    const FrameProfile& FrameHistory::worst() const
    {
        if (count == 0)
        {
            throw std::logic_error("No frame has been recorded");
        }
        const FrameProfile* worstFrame = &frames[0];
        for (std::size_t index = 1; index < count; ++index)
        {
            if (frames[index].frameSeconds > worstFrame->frameSeconds)
            {
                worstFrame = &frames[index];
            }
        }
        return *worstFrame;
    }

    float FrameHistory::averageFrameSeconds() const
    {
        if (count == 0)
        {
            return 0.0F;
        }
        float total = 0.0F;
        for (std::size_t index = 0; index < count; ++index)
        {
            total += frames[index].frameSeconds;
        }
        return total / static_cast<float>(count);
    }

    std::vector<float> FrameHistory::frameSecondsOldestFirst() const
    {
        std::vector<float> result;
        result.reserve(count);
        const std::size_t oldest = count < frames.size() ? 0 : next;
        for (std::size_t offset = 0; offset < count; ++offset)
        {
            result.push_back(frames[(oldest + offset) % frames.size()].frameSeconds);
        }
        return result;
    }
}
