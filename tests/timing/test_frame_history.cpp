#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "simple_platformer/timing/frame_profile.hpp"
#include "support/require_near.hpp"

namespace
{
    using simple_platformer::FrameHistory;
    using simple_platformer::FrameProfile;

    FrameProfile frameTaking(float seconds)
    {
        FrameProfile frame;
        frame.frameSeconds = seconds;
        return frame;
    }
}

TEST_CASE("A frame history keeps the newest frames and drops the oldest", "[timing][profile]")
{
    FrameHistory history(3);
    REQUIRE(history.size() == 0);
    REQUIRE(history.capacity() == 3);

    history.push(frameTaking(0.010F));
    history.push(frameTaking(0.020F));
    history.push(frameTaking(0.030F));
    REQUIRE(history.size() == 3);
    REQUIRE(history.frameSecondsOldestFirst() == std::vector<float>{0.010F, 0.020F, 0.030F});

    history.push(frameTaking(0.040F));
    REQUIRE(history.size() == 3);
    REQUIRE(history.frameSecondsOldestFirst() == std::vector<float>{0.020F, 0.030F, 0.040F});
    REQUIRE_NEAR(history.latest().frameSeconds, 0.040F);
}

TEST_CASE("A frame history summarises the frames it holds", "[timing][profile]")
{
    FrameHistory history(4);
    history.push(frameTaking(0.010F));
    FrameProfile slow = frameTaking(0.050F);
    slow.simulationTicks = 3;
    history.push(slow);
    history.push(frameTaking(0.030F));

    REQUIRE_NEAR(history.averageFrameSeconds(), 0.030F);
    REQUIRE_NEAR(history.worst().frameSeconds, 0.050F);
    // The worst frame comes back whole, so its breakdown can be read after it passed.
    REQUIRE(history.worst().simulationTicks == 3);
}

TEST_CASE("An empty frame history has no latest or worst frame", "[timing][profile]")
{
    const FrameHistory history;
    REQUIRE(history.averageFrameSeconds() == 0.0F);
    REQUIRE(history.frameSecondsOldestFirst().empty());
    REQUIRE_THROWS_AS(history.latest(), std::logic_error);
    REQUIRE_THROWS_AS(history.worst(), std::logic_error);
}

TEST_CASE("A frame history rejects impossible measurements", "[timing][profile]")
{
    FrameHistory history;
    REQUIRE_THROWS_AS(FrameHistory(0), std::invalid_argument);
    REQUIRE_THROWS_AS(history.push(frameTaking(-0.001F)), std::invalid_argument);
    REQUIRE_THROWS_AS(
        history.push(frameTaking(std::numeric_limits<float>::infinity())), std::invalid_argument);

    FrameProfile negativeTicks;
    negativeTicks.simulationTicks = -1;
    REQUIRE_THROWS_AS(history.push(negativeTicks), std::invalid_argument);
}

TEST_CASE("Adding to a phase sums repeats and keeps first-seen order", "[timing][profile]")
{
    FrameProfile profile;
    simple_platformer::addPhaseSeconds(profile, "Senses", 0.001F);
    simple_platformer::addPhaseSeconds(profile, "Movement", 0.002F);
    simple_platformer::addPhaseSeconds(profile, "Senses", 0.003F);

    REQUIRE(profile.phases.size() == 2);
    REQUIRE(std::string(profile.phases[0].name) == "Senses");
    REQUIRE_NEAR(profile.phases[0].seconds, 0.004F);
    REQUIRE(std::string(profile.phases[1].name) == "Movement");
    REQUIRE_THROWS_AS(
        simple_platformer::addPhaseSeconds(profile, "Senses", -0.001F), std::invalid_argument);
}

TEST_CASE("The latest simulated frame skips frames that ran no step", "[timing][profile]")
{
    FrameHistory history(4);
    REQUIRE(history.latestSimulated() == nullptr);

    FrameProfile stepped = frameTaking(0.016F);
    stepped.simulationTicks = 1;
    history.push(stepped);
    history.push(frameTaking(0.007F));
    history.push(frameTaking(0.007F));

    REQUIRE(history.latestSimulated() != nullptr);
    REQUIRE(history.latestSimulated()->simulationTicks == 1);
    REQUIRE_NEAR(history.latestSimulated()->frameSeconds, 0.016F);
    REQUIRE_NEAR(history.latest().frameSeconds, 0.007F);
}

TEST_CASE("A frame history reports one measurement across its frames", "[timing][profile]")
{
    FrameHistory history(3);
    FrameProfile first = frameTaking(0.010F);
    first.simulationSeconds = 0.004F;
    simple_platformer::addPhaseSeconds(first, "NPC behaviour", 0.003F);
    FrameProfile second = frameTaking(0.007F);
    history.push(first);
    history.push(second);

    REQUIRE(history.simulationSecondsOldestFirst() == std::vector<float>{0.004F, 0.0F});
    // A frame that ran no step contributes zero to every phase.
    REQUIRE(history.phaseSecondsOldestFirst("NPC behaviour") == std::vector<float>{0.003F, 0.0F});
    REQUIRE(history.phaseSecondsOldestFirst("Attacks") == std::vector<float>{0.0F, 0.0F});
}
