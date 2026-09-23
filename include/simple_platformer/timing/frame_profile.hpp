#pragma once

#include <cstddef>
#include <vector>

namespace simple_platformer
{
    // One named part of the simulation step and what it cost this frame, summed over the
    // fixed steps the frame ran.
    struct PhaseTiming
    {
        const char* name = "";
        float seconds = 0.0F;
    };

    // What one frame of the application cost, in wall-clock seconds. The application
    // measures the frame; the simulation measures its own phases when handed a profile.
    struct FrameProfile
    {
        float frameSeconds = 0.0F;
        // How many fixed simulation steps ran to catch up with the frame.
        int simulationTicks = 0;
        float simulationSeconds = 0.0F;
        float sceneSeconds = 0.0F;
        float renderSeconds = 0.0F;
        float interfaceSeconds = 0.0F;
        // In the order the simulation runs them.
        std::vector<PhaseTiming> phases;
        // Navigation searches are the simulation's one expensive, occasional job.
        int pathSearches = 0;
    };

    // Adds to the named phase, appending it the first time it is seen.
    void addPhaseSeconds(FrameProfile& profile, const char* name, float seconds);

    // The most recent frames, oldest dropped first, for a frame-time plot and its summary.
    class FrameHistory
    {
    public:
        explicit FrameHistory(std::size_t capacity = 120);

        void push(const FrameProfile& frame);

        std::size_t size() const;
        std::size_t capacity() const;
        // Both require at least one frame.
        const FrameProfile& latest() const;
        const FrameProfile& worst() const;
        // The newest frame that ran a simulation step, or nullptr. On a display faster than
        // the fixed step, many frames run none and have no breakdown to show.
        const FrameProfile* latestSimulated() const;
        float averageFrameSeconds() const;
        std::vector<float> frameSecondsOldestFirst() const;
        std::vector<float> simulationSecondsOldestFirst() const;
        // One phase's cost per frame; zero for frames that did not run it.
        std::vector<float> phaseSecondsOldestFirst(const char* name) const;

    private:
        std::vector<FrameProfile> frames;
        std::size_t next = 0;
        std::size_t count = 0;
    };
}
