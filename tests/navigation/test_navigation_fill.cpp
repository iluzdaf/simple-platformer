#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/platformer_connection_cache.hpp"
#include "simple_platformer/navigation/navigation_fill.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/timing/frame_profile.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/fixed_step.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    using simple_platformer::PlatformerConnectionCache;
    using simple_platformer::PlatformerTraversalProfile;

    constexpr glm::vec2 BodySize{12.0F, 12.0F};
    const PlatformerTraversalProfile Small{
        BodySize,
        simple_platformer::PlatformerMovementConfig{},
        tests::FixedStepSeconds};
    const PlatformerTraversalProfile Tall{
        {12.0F, 20.0F},
        simple_platformer::PlatformerMovementConfig{},
        tests::FixedStepSeconds};
}

TEST_CASE(
    "A level queues navigation for each walking NPC profile in its world",
    "[navigation][fill]")
{
    simple_platformer::World world;
    // Two walkers of one profile, one of another, and a flyer, which needs no connections.
    for (const float x : {24.0F, 40.0F})
    {
        world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                           .atFeet({x, 32.0F})
                           .platforming()
                           .thinking({64.0F, 1.0F}));
    }
    world.addActor(tests::ActorBuilder::sized({12.0F, 20.0F})
                       .atFeet({56.0F, 32.0F})
                       .platforming()
                       .thinking({64.0F, 1.0F}));
    world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                       .atFeet({8.0F, 16.0F})
                       .flying(20.0F)
                       .thinking({64.0F, 1.0F}));

    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::queueNavigationFill(map, world, tests::FixedStepSeconds);
    const std::vector<PlatformerTraversalProfile> profiles =
        world.platformerConnections().knownProfiles();
    REQUIRE(profiles.size() == 2);
    REQUIRE(profiles[0] == Small);
    REQUIRE(profiles[1] == Tall);
    REQUIRE_THROWS_AS(
        simple_platformer::queueNavigationFill(map, world, 0.0F), std::invalid_argument);
}

TEST_CASE("A fill caches queued cells for every known profile", "[navigation][fill]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::World world;
    world.addActor(tests::ActorBuilder::sized(Small.size)
                       .atFeet({24.0F, 32.0F})
                       .platforming()
                       .thinking({64.0F, 1.0F}));
    world.addActor(tests::ActorBuilder::sized(Tall.size)
                       .atFeet({56.0F, 32.0F})
                       .platforming()
                       .thinking({64.0F, 1.0F}));
    PlatformerConnectionCache& cache = world.platformerConnections();
    const std::size_t cells =
        static_cast<std::size_t>(map.width()) * static_cast<std::size_t>(map.height());

    // Queuing stores no connections yet, but registers both profiles.
    simple_platformer::queueNavigationFill(map, world, tests::FixedStepSeconds);
    REQUIRE(cache.size() == 0);
    REQUIRE(cache.knownProfiles().size() == 2);
    REQUIRE(cache.cellsPending(Small) == cells);
    REQUIRE(cache.cellsPending(Tall) == cells);

    // Each fill shares its budget between profiles with cells waiting.
    simple_platformer::FrameProfile frame;
    const int firstStep = simple_platformer::advanceNavigationFill(
        map, cache, simple_platformer::NavigationFillTicksPerStep, &frame);
    REQUIRE(firstStep > 0);
    REQUIRE(simple_platformer::frameStatisticCount(frame, "Fill cells cached") == firstStep);
    REQUIRE(cache.cellsPending(Small) < cells);
    REQUIRE(cache.cellsPending(Tall) < cells);
    int cached = firstStep;
    for (std::size_t step = 0; step < 2 * cells; ++step)
    {
        const int work = simple_platformer::advanceNavigationFill(
            map, cache, simple_platformer::NavigationFillTicksPerStep);
        cached += work;
        if (work == 0)
        {
            break;
        }
    }
    REQUIRE(static_cast<std::size_t>(cached) == 2 * cells);
    REQUIRE(cache.size() == 2 * cells);
    REQUIRE(cache.cellsPending(Small) == 0);
    REQUIRE(cache.cellsPending(Tall) == 0);
    REQUIRE(cache.cachedConnections({2, 1}, Tall) != nullptr);
    REQUIRE(simple_platformer::advanceNavigationFill(map, cache, 1000000) == 0);

    REQUIRE_THROWS_AS(
        simple_platformer::advanceNavigationFill(map, cache, -1), std::invalid_argument);
}

TEST_CASE("A fill caches queued cells until its budget is spent", "[navigation][cache][fill]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "###..###", "########"});
    simple_platformer::World world;
    world.addActor(tests::ActorBuilder::sized(BodySize)
                       .atFeet({24.0F, 32.0F})
                       .platforming()
                       .thinking({64.0F, 1.0F}));
    PlatformerConnectionCache& cache = world.platformerConnections();
    const PlatformerTraversalProfile profile{BodySize, {}, tests::FixedStepSeconds};
    const std::size_t cells =
        static_cast<std::size_t>(map.width()) * static_cast<std::size_t>(map.height());
    const auto fill = [&](int tickBudget, simple_platformer::FrameProfile& frame)
    { return simple_platformer::advanceNavigationFill(map, cache, tickBudget, &frame); };

    simple_platformer::queueNavigationFill(map, world, tests::FixedStepSeconds);
    REQUIRE(cache.size() == 0);
    REQUIRE(cache.cellsPending(profile) == cells);
    simple_platformer::FrameProfile noBudget;
    REQUIRE(fill(0, noBudget) == 0);

    // Even a cell with no connections spends budget. The next two cost the same.
    simple_platformer::FrameProfile firstFrame;
    REQUIRE(fill(1, firstFrame) == 1);
    REQUIRE(simple_platformer::frameStatisticCount(firstFrame, "Fill simulated ticks") == 0);
    const int firstBudgetSpent =
        simple_platformer::frameStatisticCount(firstFrame, "Fill budget spent");
    REQUIRE(firstBudgetSpent > 0);
    simple_platformer::FrameProfile secondFrame;
    REQUIRE(fill(2 * firstBudgetSpent, secondFrame) == 2);
    REQUIRE(simple_platformer::frameStatisticCount(secondFrame, "Fill simulated ticks") == 0);
    REQUIRE(
        simple_platformer::frameStatisticCount(secondFrame, "Fill budget spent") ==
        2 * firstBudgetSpent);
    REQUIRE(cache.cellsPending(profile) == cells - 3);
    REQUIRE(cache.cachedCellCount(profile) == 3);

    // The rest go with ticks to spare, and a fill with nothing waiting does nothing.
    simple_platformer::FrameProfile restFrame;
    const int rest = fill(1000000, restFrame);
    REQUIRE(static_cast<std::size_t>(rest) == cells - 3);
    const int restSimulatedTicks =
        simple_platformer::frameStatisticCount(restFrame, "Fill simulated ticks");
    REQUIRE(restSimulatedTicks > 0);
    REQUIRE(
        simple_platformer::frameStatisticCount(restFrame, "Fill budget spent") ==
        restSimulatedTicks + rest * firstBudgetSpent);
    REQUIRE(cache.size() == cells);
    REQUIRE(cache.cellsPending(profile) == 0);
    simple_platformer::FrameProfile emptyFrame;
    REQUIRE(fill(1000000, emptyFrame) == 0);

    // Queuing again with every cell cached queues nothing.
    simple_platformer::queueNavigationFill(map, world, tests::FixedStepSeconds);
    REQUIRE(cache.cellsPending(profile) == 0);

    REQUIRE_THROWS_AS(fill(-1, emptyFrame), std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::queueNavigationFill(map, world, 0.0F), std::invalid_argument);
}

TEST_CASE("A fill counts the breaks it applies and the cells they drop", "[navigation][fill]")
{
    simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "###g####"})
            .where('g', tests::Tile().blocksMovement().breaksInto('.'));
    PlatformerConnectionCache cache;
    cache.storeConnections({3, 0}, Small, {}, {{2, 0}, {4, 1}});
    cache.storeConnections({7, 0}, Small, {}, {{6, 0}, {7, 1}});

    REQUIRE(map.breakTile({3, 1}));
    simple_platformer::FrameProfile breaking;
    simple_platformer::advanceNavigationFill(map, cache, 0, &breaking);
    REQUIRE(simple_platformer::frameStatisticCount(breaking, "Tile breaks applied") == 1);
    REQUIRE(simple_platformer::frameStatisticCount(breaking, "Cells dropped") == 1);

    simple_platformer::FrameProfile quiet;
    simple_platformer::advanceNavigationFill(map, cache, 0, &quiet);
    REQUIRE(simple_platformer::frameStatisticCount(quiet, "Tile breaks applied") == 0);
    REQUIRE(simple_platformer::frameStatisticCount(quiet, "Cells dropped") == 0);
}
