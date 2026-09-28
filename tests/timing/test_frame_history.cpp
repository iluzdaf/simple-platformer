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
    simple_platformer::addPhaseSeconds(profile, "Category A", "First", 0.001F);
    simple_platformer::addPhaseSeconds(profile, "Category B", "Second", 0.002F);
    simple_platformer::addPhaseSeconds(profile, "Category A", "First", 0.003F);

    REQUIRE(profile.phases.size() == 2);
    REQUIRE(std::string(profile.phases[0].category) == "Category A");
    REQUIRE(std::string(profile.phases[0].name) == "First");
    REQUIRE_NEAR(profile.phases[0].seconds, 0.004F);
    REQUIRE(std::string(profile.phases[1].name) == "Second");
    REQUIRE_THROWS_AS(
        simple_platformer::addPhaseSeconds(profile, "Category A", "First", -0.001F),
        std::invalid_argument);
    // A phase belongs to one category.
    REQUIRE_THROWS_AS(
        simple_platformer::addPhaseSeconds(profile, "Category B", "First", 0.001F),
        std::invalid_argument);
}

TEST_CASE(
    "Phases by cost list the dearest category first, then its dearest phases",
    "[timing][profile]")
{
    FrameProfile profile;
    simple_platformer::addPhaseSeconds(profile, "Category A", "A first", 0.001F);
    simple_platformer::addPhaseSeconds(profile, "Category A", "A second", 0.003F);
    simple_platformer::addPhaseSeconds(profile, "Category B", "B only", 0.005F);
    simple_platformer::addPhaseSeconds(profile, "Category C", "C first", 0.002F);
    simple_platformer::addPhaseSeconds(profile, "Category C", "C second", 0.004F);
    // A tie keeps simulation order.
    simple_platformer::addPhaseSeconds(profile, "Category D", "D first", 0.001F);
    simple_platformer::addPhaseSeconds(profile, "Category D", "D second", 0.001F);

    const std::vector<simple_platformer::PhaseTiming> sorted =
        simple_platformer::phasesByCost(profile.phases);
    std::vector<std::string> names;
    names.reserve(sorted.size());
    for (const simple_platformer::PhaseTiming& phase : sorted)
    {
        names.emplace_back(phase.name);
    }
    // Category totals are C 6 ms, B 5, A 4, and D 2.
    REQUIRE(
        names ==
        std::vector<std::string>{
            "C second", "C first", "B only", "A second", "A first", "D first", "D second"});
    REQUIRE(simple_platformer::phasesByCost({}).empty());
}

TEST_CASE("A frame history lists every phase its frames ran, in order", "[timing][profile]")
{
    FrameHistory history(4);
    REQUIRE(history.phasesSummed().empty());

    // The second frame introduces a phase between two already seen in the first.
    FrameProfile first = frameTaking(0.016F);
    simple_platformer::addPhaseSeconds(first, "Category A", "First", 0.001F);
    simple_platformer::addPhaseSeconds(first, "Category C", "Last", 0.002F);
    FrameProfile second = frameTaking(0.016F);
    simple_platformer::addPhaseSeconds(second, "Category A", "First", 0.003F);
    simple_platformer::addPhaseSeconds(second, "Category B", "Middle", 0.004F);
    simple_platformer::addPhaseSeconds(second, "Category C", "Last", 0.005F);
    history.push(first);
    history.push(second);
    history.push(frameTaking(0.007F));

    const std::vector<simple_platformer::PhaseTiming> phases = history.phasesSummed();
    REQUIRE(phases.size() == 3);
    REQUIRE(std::string(phases[0].name) == "First");
    REQUIRE(std::string(phases[1].name) == "Middle");
    REQUIRE(std::string(phases[1].category) == "Category B");
    REQUIRE(std::string(phases[2].name) == "Last");
    REQUIRE_NEAR(phases[0].seconds, 0.004F);
    REQUIRE_NEAR(phases[1].seconds, 0.004F);
    REQUIRE_NEAR(phases[2].seconds, 0.007F);
}

TEST_CASE("A frame history reports one measurement across its frames", "[timing][profile]")
{
    FrameHistory history(3);
    FrameProfile first = frameTaking(0.010F);
    first.simulationSeconds = 0.004F;
    simple_platformer::addPhaseSeconds(first, "Category A", "First", 0.001F);
    simple_platformer::addPhaseSeconds(first, "Category A", "Second", 0.003F);
    FrameProfile second = frameTaking(0.007F);
    history.push(first);
    history.push(second);

    REQUIRE(history.simulationSecondsOldestFirst() == std::vector<float>{0.004F, 0.0F});
    // A category is the sum of its phases; a frame that ran no step contributes zero.
    REQUIRE(history.categorySecondsOldestFirst("Category A") == std::vector<float>{0.004F, 0.0F});
    REQUIRE(history.categorySecondsOldestFirst("Category B") == std::vector<float>{0.0F, 0.0F});
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

TEST_CASE("A phase scope closes when work exits early", "[timing][profile]")
{
    FrameProfile profile;
    const auto finishEarly = [&profile]
    {
        const simple_platformer::PhaseScope scope(&profile, "Category A", "Early return");
        return 7;
    };

    REQUIRE(finishEarly() == 7);
    REQUIRE(profile.phases.size() == 1);
    REQUIRE(std::string(profile.phases[0].name) == "Early return");
    REQUIRE(profile.phases[0].seconds >= 0.0F);
    REQUIRE(profile.nestedSecondsOfOpenPhases.empty());

    REQUIRE_THROWS_AS(
        simple_platformer::timePhase(
            &profile, "Category A", "Throwing", [] { throw std::runtime_error("failed"); }),
        std::runtime_error);
    REQUIRE(profile.nestedSecondsOfOpenPhases.empty());
}

TEST_CASE("Timing a phase charges it less the phases timed inside it", "[timing][profile]")
{
    bool ran = false;
    simple_platformer::timePhase(nullptr, "Category A", "Outer", [&ran] { ran = true; });
    REQUIRE(ran);

    FrameProfile profile;
    simple_platformer::timePhase(
        &profile,
        "Category A",
        "Outer",
        [&]
        {
            simple_platformer::timePhase(
                &profile,
                "Category A",
                "Inner",
                [] { tests::spinFor(std::chrono::milliseconds(2)); });
        });

    // The outer phase lists first though it finished last, and keeps only its own time.
    REQUIRE(profile.phases.size() == 2);
    REQUIRE(std::string(profile.phases[0].name) == "Outer");
    REQUIRE(std::string(profile.phases[1].name) == "Inner");
    REQUIRE(profile.phases[1].seconds >= 0.002F);
    REQUIRE(profile.phases[0].seconds < profile.phases[1].seconds);
    REQUIRE(profile.nestedSecondsOfOpenPhases.empty());
}
