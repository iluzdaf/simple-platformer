#include <catch2/catch_test_macros.hpp>

#include <chrono>

#include "simple_platformer/timing/stopwatch.hpp"
#include "support/spin_for.hpp"

namespace
{
    using simple_platformer::Stopwatch;
    using tests::spinFor;
}

TEST_CASE("A stopwatch counts the seconds since it started", "[timing][stopwatch]")
{
    const Stopwatch stopwatch;
    REQUIRE(stopwatch.elapsedSeconds() >= 0.0F);

    spinFor(std::chrono::milliseconds(2));
    REQUIRE(stopwatch.elapsedSeconds() >= 0.002F);
}

TEST_CASE("A lap returns the time since the last lap and restarts", "[timing][stopwatch]")
{
    Stopwatch stopwatch;
    spinFor(std::chrono::milliseconds(5));

    const float lap = stopwatch.lapSeconds();
    REQUIRE(lap >= 0.005F);
    // The lap restarted the stopwatch, so far less has passed since.
    REQUIRE(stopwatch.elapsedSeconds() < lap);

    spinFor(std::chrono::milliseconds(2));
    const float next = stopwatch.lapSeconds();
    REQUIRE(next >= 0.002F);
    REQUIRE(next < lap);
}
