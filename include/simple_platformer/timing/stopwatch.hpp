#pragma once

#include <chrono>

namespace simple_platformer
{
    // Wall-clock seconds since it started. This is the one place the engine reads a
    // clock, so everything else can be timed without owning one.
    class Stopwatch
    {
    public:
        // Starts now.
        Stopwatch();

        float elapsedSeconds() const;
        // Restarts and returns the seconds since it last started: read once per frame,
        // that is the frame's time.
        float lapSeconds();

    private:
        std::chrono::steady_clock::time_point start;
    };
}
