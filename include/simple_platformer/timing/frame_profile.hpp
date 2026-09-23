#pragma once

#include <cstddef>
#include <functional>
#include <vector>

namespace simple_platformer
{
    // One named part of the simulation step and what it cost this frame, summed over the
    // fixed steps the frame ran. The category is the broad part of the step it belongs to,
    // such as "NPC" or "Combat", so a plot can show a few bands instead of every phase.
    struct PhaseTiming
    {
        const char* category = "";
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
        // Navigation searches are the simulation's one expensive, occasional job: how many
        // ran, the cells they expanded, and the movement ticks they simulated to build
        // platformer connections.
        int pathSearches = 0;
        int pathSearchNodes = 0;
        int pathSearchSimulatedTicks = 0;
        // Bookkeeping for timePhase: for each phase being timed right now, outermost
        // first, the seconds already charged to phases timed inside it. Empty between steps.
        std::vector<float> nestedSecondsOfOpenPhases;
    };

    // Runs one phase of the step and, when there is a profile, charges its wall-clock time
    // to it less any phases timed inside it, so sibling phases never count the same time
    // twice. This is the one place the engine reads a clock.
    void timePhase(
        FrameProfile* profile,
        const char* category,
        const char* name,
        const std::function<void()>& phase);

    // Adds to the named phase, appending it the first time it is seen. A phase keeps the
    // category it was first charged under.
    void addPhaseSeconds(
        FrameProfile& profile,
        const char* category,
        const char* name,
        float seconds);

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
        // Summed over every frame held, for costs per simulation step.
        int totalSimulationTicks() const;
        int totalPathSearches() const;
        int totalPathSearchNodes() const;
        int totalPathSearchSimulatedTicks() const;
        std::vector<float> frameSecondsOldestFirst() const;
        std::vector<float> simulationSecondsOldestFirst() const;
        // One phase's cost per frame; zero for frames that did not run it.
        std::vector<float> phaseSecondsOldestFirst(const char* name) const;
        // Every phase in one category summed, per frame.
        std::vector<float> categorySecondsOldestFirst(const char* category) const;

    private:
        std::vector<FrameProfile> frames;
        std::size_t next = 0;
        std::size_t count = 0;
    };
}
