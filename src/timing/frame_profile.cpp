#include "simple_platformer/timing/frame_profile.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "simple_platformer/math/validation.hpp"

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

    void addFrameStatistic(FrameProfile* profile, const char* category, const char* name, int count)
    {
        if (profile == nullptr)
        {
            return;
        }
        if (count < 0)
        {
            throw std::invalid_argument(std::string("Statistic ") + name + " cannot be negative");
        }
        for (FrameStatistic& statistic : profile->statistics)
        {
            if (std::string_view(statistic.name) != name)
            {
                continue;
            }
            if (std::string_view(statistic.category) != category)
            {
                throw std::invalid_argument(
                    std::string("Statistic ") + name + " is already charged to " +
                    statistic.category);
            }
            if (count > std::numeric_limits<int>::max() - statistic.count)
            {
                throw std::overflow_error(std::string("Statistic ") + name + " is too large");
            }
            statistic.count += count;
            return;
        }
        profile->statistics.push_back({category, name, count});
    }

    int frameStatisticCount(const FrameProfile& profile, const char* name)
    {
        const auto found = std::find_if(
            profile.statistics.begin(),
            profile.statistics.end(),
            [name](const FrameStatistic& statistic)
            { return std::string_view(statistic.name) == name; });
        return found == profile.statistics.end() ? 0 : found->count;
    }

    PhaseScope::PhaseScope(FrameProfile* profile, const char* category, const char* name)
        : profile(profile)
    {
        if (profile == nullptr)
        {
            return;
        }
        // Register before running so an outer phase lists ahead of nested phases.
        addPhaseSeconds(*profile, category, name, 0.0F);
        const auto phase = std::find_if(
            profile->phases.begin(),
            profile->phases.end(),
            [name](const PhaseTiming& timing) { return std::string_view(timing.name) == name; });
        phaseIndex = static_cast<std::size_t>(phase - profile->phases.begin());
        profile->nestedSecondsOfOpenPhases.push_back(0.0F);
        stopwatch.emplace();
    }

    PhaseScope::~PhaseScope() noexcept
    {
        if (profile == nullptr || !stopwatch.has_value())
        {
            return;
        }
        const float elapsed = stopwatch->elapsedSeconds();
        const float nested = profile->nestedSecondsOfOpenPhases.back();
        profile->nestedSecondsOfOpenPhases.pop_back();
        profile->phases[phaseIndex].seconds += elapsed > nested ? elapsed - nested : 0.0F;
        if (!profile->nestedSecondsOfOpenPhases.empty())
        {
            profile->nestedSecondsOfOpenPhases.back() += elapsed;
        }
    }

    std::vector<PhaseTiming> phasesByCost(const std::vector<PhaseTiming>& phases)
    {
        struct CategoryCost
        {
            std::string_view name;
            float seconds = 0.0F;
        };

        std::vector<CategoryCost> categories;
        for (const PhaseTiming& phase : phases)
        {
            const auto category = std::find_if(
                categories.begin(),
                categories.end(),
                [&](const CategoryCost& cost) { return cost.name == phase.category; });
            if (category == categories.end())
            {
                categories.push_back({phase.category, phase.seconds});
                continue;
            }
            category->seconds += phase.seconds;
        }
        std::stable_sort(
            categories.begin(),
            categories.end(),
            [](const CategoryCost& left, const CategoryCost& right)
            { return left.seconds > right.seconds; });

        std::vector<PhaseTiming> sorted;
        sorted.reserve(phases.size());
        for (const CategoryCost& category : categories)
        {
            const auto first = sorted.end() - sorted.begin();
            for (const PhaseTiming& phase : phases)
            {
                if (std::string_view(phase.category) == category.name)
                {
                    sorted.push_back(phase);
                }
            }
            std::stable_sort(
                sorted.begin() + first,
                sorted.end(),
                [](const PhaseTiming& left, const PhaseTiming& right)
                { return left.seconds > right.seconds; });
        }
        return sorted;
    }

    void timePhase(
        FrameProfile* profile,
        const char* category,
        const char* name,
        const std::function<void()>& phase)
    {
        const PhaseScope scope(profile, category, name);
        phase();
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
            throw std::invalid_argument("A frame cannot run a negative number of steps");
        }
        for (const FrameStatistic& statistic : frame.statistics)
        {
            if (statistic.count < 0)
            {
                throw std::invalid_argument("A frame cannot record a negative statistic");
            }
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

    const FrameProfile& FrameHistory::frameOldestFirst(std::size_t index) const
    {
        if (index >= count)
        {
            throw std::out_of_range("No frame has been recorded at that position");
        }
        const std::size_t oldest = count < frames.size() ? 0 : next;
        return frames[(oldest + index) % frames.size()];
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

    std::vector<FrameStatistic> FrameHistory::statisticsSummed() const
    {
        std::vector<FrameStatistic> summed;
        for (std::size_t index = 0; index < count; ++index)
        {
            for (const FrameStatistic& statistic : frames[index].statistics)
            {
                const auto existing = std::find_if(
                    summed.begin(),
                    summed.end(),
                    [&statistic](const FrameStatistic& entry)
                    { return std::string_view(entry.name) == statistic.name; });
                if (existing == summed.end())
                {
                    summed.push_back(statistic);
                    continue;
                }
                if (std::string_view(existing->category) != statistic.category)
                {
                    throw std::invalid_argument("A statistic changed category between frames");
                }
                if (statistic.count > std::numeric_limits<int>::max() - existing->count)
                {
                    throw std::overflow_error("A statistic total is too large");
                }
                existing->count += statistic.count;
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
