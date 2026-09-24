#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "simple_platformer/timing/frame_profile.hpp"
#include "support/require_near.hpp"
#include "support/spin_for.hpp"

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
    REQUIRE_NEAR(history.frameOldestFirst(0).frameSeconds, 0.020F);
    REQUIRE_NEAR(history.frameOldestFirst(2).frameSeconds, 0.040F);
    REQUIRE_THROWS_AS(history.frameOldestFirst(3), std::out_of_range);
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
    FrameProfile negativeCells;
    negativeCells.pathSearchNodes = -1;
    REQUIRE_THROWS_AS(history.push(negativeCells), std::invalid_argument);
    FrameProfile negativeReuse;
    negativeReuse.pathSearchCellsReused = -1;
    REQUIRE_THROWS_AS(history.push(negativeReuse), std::invalid_argument);
    FrameProfile negativeMemory;
    negativeMemory.pathSearchesRemembered = -1;
    REQUIRE_THROWS_AS(history.push(negativeMemory), std::invalid_argument);
    FrameProfile stillTiming;
    stillTiming.nestedSecondsOfOpenPhases.push_back(0.0F);
    REQUIRE_THROWS_AS(history.push(stillTiming), std::invalid_argument);
}

TEST_CASE("Adding to a phase sums repeats and keeps first-seen order", "[timing][profile]")
{
    FrameProfile profile;
    simple_platformer::addPhaseSeconds(profile, "NPC", "Senses", 0.001F);
    simple_platformer::addPhaseSeconds(profile, "Movement", "Actors", 0.002F);
    simple_platformer::addPhaseSeconds(profile, "NPC", "Senses", 0.003F);

    REQUIRE(profile.phases.size() == 2);
    REQUIRE(std::string(profile.phases[0].category) == "NPC");
    REQUIRE(std::string(profile.phases[0].name) == "Senses");
    REQUIRE_NEAR(profile.phases[0].seconds, 0.004F);
    REQUIRE(std::string(profile.phases[1].name) == "Actors");
    REQUIRE_THROWS_AS(
        simple_platformer::addPhaseSeconds(profile, "NPC", "Senses", -0.001F),
        std::invalid_argument);
    // A phase belongs to one category.
    REQUIRE_THROWS_AS(
        simple_platformer::addPhaseSeconds(profile, "Combat", "Senses", 0.001F),
        std::invalid_argument);
}

TEST_CASE(
    "Phases by cost list the dearest category first, then its dearest phases",
    "[timing][profile]")
{
    FrameProfile profile;
    simple_platformer::addPhaseSeconds(profile, "NPC", "NPC senses", 0.001F);
    simple_platformer::addPhaseSeconds(profile, "NPC", "NPC behaviour", 0.003F);
    simple_platformer::addPhaseSeconds(profile, "Movement", "Actor movement", 0.005F);
    simple_platformer::addPhaseSeconds(profile, "Combat", "Attacks", 0.002F);
    simple_platformer::addPhaseSeconds(profile, "Combat", "Projectiles", 0.004F);
    // A tie keeps simulation order.
    simple_platformer::addPhaseSeconds(profile, "World", "Pickups", 0.001F);
    simple_platformer::addPhaseSeconds(profile, "World", "Level exit", 0.001F);

    const std::vector<simple_platformer::PhaseTiming> sorted =
        simple_platformer::phasesByCost(profile.phases);
    std::vector<std::string> names;
    names.reserve(sorted.size());
    for (const simple_platformer::PhaseTiming& phase : sorted)
    {
        names.emplace_back(phase.name);
    }
    // Combat 6 ms, Movement 5, NPC 4, World 2.
    REQUIRE(
        names == std::vector<std::string>{
                     "Projectiles",
                     "Attacks",
                     "Actor movement",
                     "NPC behaviour",
                     "NPC senses",
                     "Pickups",
                     "Level exit"});
    REQUIRE(simple_platformer::phasesByCost({}).empty());
}

TEST_CASE("A frame history lists every phase its frames ran, in order", "[timing][profile]")
{
    FrameHistory history(4);
    REQUIRE(history.phasesSummed().empty());

    // The first frame ran no search; the second did, between behaviour and movement.
    FrameProfile first = frameTaking(0.016F);
    simple_platformer::addPhaseSeconds(first, "NPC", "NPC behaviour", 0.001F);
    simple_platformer::addPhaseSeconds(first, "Movement", "Actor movement", 0.002F);
    FrameProfile second = frameTaking(0.016F);
    simple_platformer::addPhaseSeconds(second, "NPC", "NPC behaviour", 0.003F);
    simple_platformer::addPhaseSeconds(second, "NPC", "Path search", 0.004F);
    simple_platformer::addPhaseSeconds(second, "Movement", "Actor movement", 0.005F);
    history.push(first);
    history.push(second);
    history.push(frameTaking(0.007F));

    const std::vector<simple_platformer::PhaseTiming> phases = history.phasesSummed();
    REQUIRE(phases.size() == 3);
    REQUIRE(std::string(phases[0].name) == "NPC behaviour");
    REQUIRE(std::string(phases[1].name) == "Path search");
    REQUIRE(std::string(phases[1].category) == "NPC");
    REQUIRE(std::string(phases[2].name) == "Actor movement");
    REQUIRE_NEAR(phases[0].seconds, 0.004F);
    REQUIRE_NEAR(phases[1].seconds, 0.004F);
    REQUIRE_NEAR(phases[2].seconds, 0.007F);
}

TEST_CASE("A frame history reports one measurement across its frames", "[timing][profile]")
{
    FrameHistory history(3);
    FrameProfile first = frameTaking(0.010F);
    first.simulationSeconds = 0.004F;
    simple_platformer::addPhaseSeconds(first, "NPC", "NPC senses", 0.001F);
    simple_platformer::addPhaseSeconds(first, "NPC", "NPC behaviour", 0.003F);
    FrameProfile second = frameTaking(0.007F);
    history.push(first);
    history.push(second);

    REQUIRE(history.simulationSecondsOldestFirst() == std::vector<float>{0.004F, 0.0F});
    // A category is the sum of its phases; a frame that ran no step contributes zero.
    REQUIRE(history.categorySecondsOldestFirst("NPC") == std::vector<float>{0.004F, 0.0F});
    REQUIRE(history.categorySecondsOldestFirst("Combat") == std::vector<float>{0.0F, 0.0F});
}

TEST_CASE("A frame history totals ticks and path searches across its frames", "[timing][profile]")
{
    FrameHistory history(2);
    FrameProfile first = frameTaking(0.016F);
    first.simulationTicks = 2;
    first.pathSearches = 1;
    first.pathSearchesRemembered = 1;
    first.pathSearchesDeferred = 1;
    first.pathSearchNodes = 10;
    first.pathSearchCellsReused = 4;
    first.pathSearchSimulatedTicks = 100;
    first.navigationFillTicks = 30;
    FrameProfile second = frameTaking(0.016F);
    second.simulationTicks = 1;
    second.pathSearches = 3;
    second.pathSearchesRemembered = 2;
    second.pathSearchesDeferred = 0;
    second.pathSearchNodes = 5;
    second.pathSearchCellsReused = 5;
    second.pathSearchSimulatedTicks = 50;
    second.navigationFillTicks = 20;
    history.push(first);
    history.push(second);
    REQUIRE(history.totalSimulationTicks() == 3);
    REQUIRE(history.totalPathSearches() == 4);
    REQUIRE(history.totalPathSearchesRemembered() == 3);
    REQUIRE(history.totalPathSearchesDeferred() == 1);
    REQUIRE(history.totalPathSearchNodes() == 15);
    REQUIRE(history.totalPathSearchCellsReused() == 9);
    REQUIRE(history.totalPathSearchSimulatedTicks() == 150);
    REQUIRE(history.totalNavigationFillTicks() == 50);

    // A third frame evicts the first.
    history.push(frameTaking(0.007F));
    REQUIRE(history.totalSimulationTicks() == 1);
    REQUIRE(history.totalPathSearches() == 3);
}

TEST_CASE(
    "Seconds a system measured itself charge the phase they ran inside less",
    "[timing][profile]")
{
    FrameProfile profile;
    simple_platformer::timePhase(
        &profile,
        "NPC",
        "Outer",
        [&profile]
        {
            simple_platformer::addNestedPhaseSeconds(profile, "NPC", "Inner", 0.25F);
            simple_platformer::addNestedPhaseSeconds(profile, "NPC", "Inner", 0.25F);
        });
    REQUIRE(profile.phases.size() == 2);
    REQUIRE(std::string(profile.phases[0].name) == "Outer");
    REQUIRE(std::string(profile.phases[1].name) == "Inner");
    REQUIRE(profile.phases[1].seconds == 0.5F);
    // The outer phase took far less than the half second charged inside it, so it keeps
    // nothing, never a negative time.
    REQUIRE(profile.phases[0].seconds == 0.0F);
    // Outside any phase, the seconds are simply added.
    simple_platformer::addNestedPhaseSeconds(profile, "NPC", "Inner", 0.25F);
    REQUIRE(profile.phases[1].seconds == 0.75F);
}

TEST_CASE("Timing a phase charges it less the phases timed inside it", "[timing][profile]")
{
    bool ran = false;
    simple_platformer::timePhase(nullptr, "NPC", "Outer", [&ran] { ran = true; });
    REQUIRE(ran);

    FrameProfile profile;
    simple_platformer::timePhase(
        &profile,
        "NPC",
        "Outer",
        [&]
        {
            simple_platformer::timePhase(
                &profile, "NPC", "Inner", [] { tests::spinFor(std::chrono::milliseconds(2)); });
        });

    // The outer phase lists first though it finished last, and keeps only its own time.
    REQUIRE(profile.phases.size() == 2);
    REQUIRE(std::string(profile.phases[0].name) == "Outer");
    REQUIRE(std::string(profile.phases[1].name) == "Inner");
    REQUIRE(profile.phases[1].seconds >= 0.002F);
    REQUIRE(profile.phases[0].seconds < profile.phases[1].seconds);
    REQUIRE(profile.nestedSecondsOfOpenPhases.empty());
}
