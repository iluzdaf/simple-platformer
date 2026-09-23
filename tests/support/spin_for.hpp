#pragma once

#include <chrono>

namespace tests
{
    // Busy-waits so a timed piece of code measurably takes at least this long. Sleeping
    // would do as well for the clock, but a spin keeps the timing tests independent of
    // how promptly the scheduler wakes a thread.
    inline void spinFor(std::chrono::milliseconds duration)
    {
        const auto until = std::chrono::steady_clock::now() + duration;
        while (std::chrono::steady_clock::now() < until)
        {
        }
    }
}
