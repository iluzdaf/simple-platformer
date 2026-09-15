#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <limits>
#include <stdexcept>
#include <vector>

#include "simple_platformer/timing/fixed_step.hpp"

namespace
{
    using Catch::Matchers::WithinAbs;
    using simple_platformer::FixedDeltaSeconds;
    using simple_platformer::FixedStep;
    using simple_platformer::FixedStepResult;
}

TEST_CASE("Elapsed time is simulated in fixed 60 Hz updates", "[timing][fixed-step]")
{
    FixedStep clock;
    std::vector<float> receivedSteps;

    const FixedStepResult result =
        clock.advance(1.0 / 30.0, [&receivedSteps](float step) { receivedSteps.push_back(step); });

    REQUIRE(result.updates == 2);
    REQUIRE(receivedSteps.size() == 2);
    REQUIRE_THAT(
        receivedSteps.front(), WithinAbs(static_cast<float>(FixedDeltaSeconds), 0.000001F));
    REQUIRE_THAT(result.interpolation, WithinAbs(0.0, 0.000001));
}

TEST_CASE("A partial update remains in the accumulator", "[timing][fixed-step]")
{
    FixedStep clock;
    std::size_t updates = 0;

    const FixedStepResult first =
        clock.advance(FixedDeltaSeconds * 0.25, [&updates](float) { ++updates; });
    const FixedStepResult second =
        clock.advance(FixedDeltaSeconds * 0.75, [&updates](float) { ++updates; });

    REQUIRE(first.updates == 0);
    REQUIRE_THAT(first.interpolation, WithinAbs(0.25, 0.000001));
    REQUIRE(second.updates == 1);
    REQUIRE(updates == 1);
    REQUIRE_THAT(second.interpolation, WithinAbs(0.0, 0.000001));
}

TEST_CASE("Long frames are clamped before catch-up", "[timing][fixed-step]")
{
    FixedStep clock;
    std::size_t updates = 0;

    const FixedStepResult result = clock.advance(5.0, [&updates](float) { ++updates; });

    REQUIRE(result.updates == 15);
    REQUIRE(updates == 15);
}

TEST_CASE("Negative frame time contributes nothing", "[timing][fixed-step]")
{
    FixedStep clock;

    const FixedStepResult result = clock.advance(-1.0, [](float) {});

    REQUIRE(result.updates == 0);
    REQUIRE(result.interpolation == 0.0);
}

TEST_CASE("Reset discards pending simulation time", "[timing][fixed-step]")
{
    FixedStep clock;
    clock.advance(FixedDeltaSeconds * 0.5, [](float) {});

    clock.reset();

    REQUIRE(clock.pendingSeconds() == 0.0);
}

TEST_CASE("Invalid timing configuration is rejected", "[timing][fixed-step]")
{
    REQUIRE_THROWS_AS(FixedStep(0.0), std::invalid_argument);
    REQUIRE_THROWS_AS(FixedStep(-FixedDeltaSeconds), std::invalid_argument);
    REQUIRE_THROWS_AS(FixedStep(FixedDeltaSeconds, -1.0), std::invalid_argument);
    REQUIRE_THROWS_AS(FixedStep(std::numeric_limits<double>::infinity()), std::invalid_argument);
}

TEST_CASE("Non-finite frame time is rejected", "[timing][fixed-step]")
{
    FixedStep clock;

    REQUIRE_THROWS_AS(
        clock.advance(std::numeric_limits<double>::quiet_NaN(), [](float) {}),
        std::invalid_argument);
}
