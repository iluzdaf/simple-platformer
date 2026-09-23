#include "simple_platformer/timing/stopwatch.hpp"

#include <chrono>

namespace simple_platformer
{
    Stopwatch::Stopwatch()
        : start(std::chrono::steady_clock::now())
    {
    }

    float Stopwatch::elapsedSeconds() const
    {
        return std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
    }

    float Stopwatch::lapSeconds()
    {
        const auto now = std::chrono::steady_clock::now();
        const float elapsed = std::chrono::duration<float>(now - start).count();
        start = now;
        return elapsed;
    }
}
