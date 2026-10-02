#pragma once

#include <chrono>

namespace simple_platformer
{
    // Measures real elapsed seconds with a steady clock. Gameplay systems receive
    // deltaTime instead, so tests can advance them without waiting.
    class Stopwatch
    {
    public:
        Stopwatch();

        float elapsedSeconds() const noexcept;
        // Returns seconds since construction or the last lap, then restarts the clock.
        // The application calls this once per frame to measure frame time.
        float lapSeconds();

    private:
        std::chrono::steady_clock::time_point start;
    };
}
