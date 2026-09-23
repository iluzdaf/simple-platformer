#pragma once

#include <cstddef>
#include <vector>

namespace simple_platformer
{
    // What one frame of the application cost, in wall-clock seconds. The application
    // measures these; nothing here reads a clock, so a test can build one by hand.
    struct FrameProfile
    {
        float frameSeconds = 0.0F;
        // How many fixed simulation steps ran to catch up with the frame.
        int simulationTicks = 0;
        float simulationSeconds = 0.0F;
        float sceneSeconds = 0.0F;
        float renderSeconds = 0.0F;
        float interfaceSeconds = 0.0F;
    };

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
        float averageFrameSeconds() const;
        std::vector<float> frameSecondsOldestFirst() const;

    private:
        std::vector<FrameProfile> frames;
        std::size_t next = 0;
        std::size_t count = 0;
    };
}
