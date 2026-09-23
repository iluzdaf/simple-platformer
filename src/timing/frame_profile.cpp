#include "simple_platformer/timing/frame_profile.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "simple_platformer/math/validation.hpp"

namespace simple_platformer
{
    void addPhaseSeconds(FrameProfile& profile, const char* name, float seconds)
    {
        requireSeconds(seconds, name);
        for (PhaseTiming& phase : profile.phases)
        {
            if (std::string_view(phase.name) == name)
            {
                phase.seconds += seconds;
                return;
            }
        }
        profile.phases.push_back({name, seconds});
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
        if (frame.simulationTicks < 0 || frame.pathSearches < 0)
        {
            throw std::invalid_argument(
                "A frame cannot run a negative number of steps or searches");
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

    const FrameProfile* FrameHistory::latestSimulated() const
    {
        for (std::size_t back = 1; back <= count; ++back)
        {
            const FrameProfile& frame = frames[(next + frames.size() - back) % frames.size()];
            if (frame.simulationTicks > 0)
            {
                return &frame;
            }
        }
        return nullptr;
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

    namespace
    {
        std::vector<float> oldestFirst(
            const std::vector<FrameProfile>& frames,
            std::size_t next,
            std::size_t count,
            const std::function<float(const FrameProfile&)>& measure)
        {
            std::vector<float> result;
            result.reserve(count);
            const std::size_t oldest = count < frames.size() ? 0 : next;
            for (std::size_t offset = 0; offset < count; ++offset)
            {
                result.push_back(measure(frames[(oldest + offset) % frames.size()]));
            }
            return result;
        }
    }

    std::vector<float> FrameHistory::frameSecondsOldestFirst() const
    {
        return oldestFirst(
            frames, next, count, [](const FrameProfile& frame) { return frame.frameSeconds; });
    }

    std::vector<float> FrameHistory::simulationSecondsOldestFirst() const
    {
        return oldestFirst(
            frames, next, count, [](const FrameProfile& frame) { return frame.simulationSeconds; });
    }

    std::vector<float> FrameHistory::phaseSecondsOldestFirst(const char* name) const
    {
        return oldestFirst(
            frames,
            next,
            count,
            [name](const FrameProfile& frame)
            {
                for (const PhaseTiming& phase : frame.phases)
                {
                    if (std::string_view(phase.name) == name)
                    {
                        return phase.seconds;
                    }
                }
                return 0.0F;
            });
    }
}
