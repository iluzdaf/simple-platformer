#include <catch2/catch_test_macros.hpp>

#include <chrono>

#include "simple_platformer/timing/stopwatch.hpp"
#include "support/spin_for.hpp"

namespace
{
    using simple_platformer::Stopwatch;
    using tests::spinFor;
}

// Elapsed time

TEST_CASE("A stopwatch counts the seconds since it started", "[timing][stopwatch]")
{
    const Stopwatch stopwatch;
    REQUIRE(stopwatch.elapsedSeconds() >= 0.0F);

    spinFor(std::chrono::milliseconds(2));
    REQUIRE(stopwatch.elapsedSeconds() >= 0.002F);
}

// Laps and restarting

TEST_CASE("A lap returns the time since the last lap and restarts", "[timing][stopwatch]")
{
    Stopwatch stopwatch;
    spinFor(std::chrono::milliseconds(5));

    const auto beforeLap = std::chrono::steady_clock::now();
    const float lap = stopwatch.lapSeconds();
    const float sinceLap = stopwatch.elapsedSeconds();
    const auto afterReading = std::chrono::steady_clock::now();
    REQUIRE(lap >= 0.005F);
    // The restarted clock cannot include time before the lap call. This bound remains
    // valid even if the operating system pauses the test between calls.
    const float readingWindow = std::chrono::duration<float>(afterReading - beforeLap).count();
    REQUIRE(sinceLap <= readingWindow);

    spinFor(std::chrono::milliseconds(2));
    const float next = stopwatch.lapSeconds();
    REQUIRE(next >= 0.002F);
}
