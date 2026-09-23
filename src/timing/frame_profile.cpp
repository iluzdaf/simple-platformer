#include "simple_platformer/timing/frame_profile.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/timing/stopwatch.hpp"

namespace simple_platformer
{
    namespace
    {
        int sumOver(
            const std::vector<FrameProfile>& frames,
            std::size_t count,
            const std::function<int(const FrameProfile&)>& measure)
        {
            int total = 0;
            for (std::size_t index = 0; index < count; ++index)
            {
                total += measure(frames[index]);
            }
            return total;
        }

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

    void addPhaseSeconds(
        FrameProfile& profile,
        const char* category,
        const char* name,
        float seconds)
    {
        requireSeconds(seconds, name);
        for (PhaseTiming& phase : profile.phases)
        {
            if (std::string_view(phase.name) == name)
            {
                if (std::string_view(phase.category) != category)
                {
                    throw std::invalid_argument(
                        std::string("Phase ") + name + " is already charged to " + phase.category);
                }
                phase.seconds += seconds;
                return;
            }
        }
        profile.phases.push_back({category, name, seconds});
    }

    void timePhase(
        FrameProfile* profile,
        const char* category,
        const char* name,
        const std::function<void()>& phase)
    {
        if (profile == nullptr)
        {
            phase();
            return;
        }
        // Registered before it runs so it lists ahead of the phases timed inside it.
        addPhaseSeconds(*profile, category, name, 0.0F);
        profile->nestedSecondsOfOpenPhases.push_back(0.0F);
        const Stopwatch stopwatch;
        phase();
        const float elapsed = stopwatch.elapsedSeconds();
        const float nested = profile->nestedSecondsOfOpenPhases.back();
        profile->nestedSecondsOfOpenPhases.pop_back();
        addPhaseSeconds(*profile, category, name, std::max(0.0F, elapsed - nested));
        if (!profile->nestedSecondsOfOpenPhases.empty())
        {
            profile->nestedSecondsOfOpenPhases.back() += elapsed;
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
        if (frame.simulationTicks < 0 || frame.pathSearches < 0 || frame.pathSearchNodes < 0 ||
            frame.pathSearchSimulatedTicks < 0)
        {
            throw std::invalid_argument(
                "A frame cannot run a negative number of steps, searches, cells or ticks");
        }
        if (!frame.nestedSecondsOfOpenPhases.empty())
        {
            throw std::invalid_argument("A frame cannot be recorded while a phase is being timed");
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

    std::vector<PhaseTiming> FrameHistory::phasesSummed() const
    {
        std::vector<PhaseTiming> summed;
        for (std::size_t index = 0; index < count; ++index)
        {
            // A phase this frame ran that no earlier frame did slots in after the phase
            // that preceded it here, so the merged list keeps the simulation's order.
            std::size_t insertAt = 0;
            for (const PhaseTiming& phase : frames[index].phases)
            {
                std::size_t existing = 0;
                while (existing < summed.size() &&
                       std::string_view(summed[existing].name) != phase.name)
                {
                    ++existing;
                }
                if (existing < summed.size())
                {
                    summed[existing].seconds += phase.seconds;
                    insertAt = existing + 1;
                    continue;
                }
                summed.insert(summed.begin() + static_cast<std::ptrdiff_t>(insertAt), phase);
                ++insertAt;
            }
        }
        return summed;
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

    int FrameHistory::totalSimulationTicks() const
    {
        return sumOver(
            frames, count, [](const FrameProfile& frame) { return frame.simulationTicks; });
    }

    int FrameHistory::totalPathSearches() const
    {
        return sumOver(frames, count, [](const FrameProfile& frame) { return frame.pathSearches; });
    }

    int FrameHistory::totalPathSearchNodes() const
    {
        return sumOver(
            frames, count, [](const FrameProfile& frame) { return frame.pathSearchNodes; });
    }

    int FrameHistory::totalPathSearchSimulatedTicks() const
    {
        return sumOver(
            frames,
            count,
            [](const FrameProfile& frame) { return frame.pathSearchSimulatedTicks; });
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

    std::vector<float> FrameHistory::categorySecondsOldestFirst(const char* category) const
    {
        return oldestFirst(
            frames,
            next,
            count,
            [category](const FrameProfile& frame)
            {
                float total = 0.0F;
                for (const PhaseTiming& phase : frame.phases)
                {
                    if (std::string_view(phase.category) == category)
                    {
                        total += phase.seconds;
                    }
                }
                return total;
            });
    }
}
