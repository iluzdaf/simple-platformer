#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/connection_cache.hpp"
#include "simple_platformer/navigation/navigation_fill.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/fixed_step.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    using simple_platformer::ConnectionBody;

    const ConnectionBody Small{
        {12.0F, 12.0F},
        simple_platformer::PlatformerMovementConfig{},
        tests::FixedStepSeconds};
    const ConnectionBody Tall{
        {12.0F, 20.0F},
        simple_platformer::PlatformerMovementConfig{},
        tests::FixedStepSeconds};
}

TEST_CASE("A level queues navigation for each walking NPC body in its world", "[navigation][fill]")
{
    simple_platformer::World world;
    // Two walkers of one body, one of another, and a flyer, which needs no connections.
    for (const float x : {24.0F, 40.0F})
    {
        world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                           .atFeet({x, 32.0F})
                           .walking()
                           .thinking({64.0F, 1.0F}));
    }
    world.addActor(tests::ActorBuilder::sized({12.0F, 20.0F})
                       .atFeet({56.0F, 32.0F})
                       .walking()
                       .thinking({64.0F, 1.0F}));
    world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                       .atFeet({8.0F, 16.0F})
                       .flying(20.0F)
                       .thinking({64.0F, 1.0F}));

    const std::vector<ConnectionBody> bodies =
        simple_platformer::platformerBodiesIn(world, tests::FixedStepSeconds);
    REQUIRE(bodies.size() == 2);
    REQUIRE(bodies[0] == Small);
    REQUIRE(bodies[1] == Tall);
    REQUIRE_THROWS_AS(simple_platformer::platformerBodiesIn(world, 0.0F), std::invalid_argument);
}

TEST_CASE("A fill keeps queued cells for every body the cache knows", "[navigation][fill]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::PlatformerConnectionCache cache;
    const std::size_t cells =
        static_cast<std::size_t>(map.width()) * static_cast<std::size_t>(map.height());

    // Queuing keeps nothing yet, and tells the cache the bodies.
    simple_platformer::queueNavigation(map, {Small, Tall}, cache);
    REQUIRE(cache.size() == 0);
    REQUIRE(cache.bodiesKept().size() == 2);
    REQUIRE(cache.cellsPending(Small) == cells);
    REQUIRE(cache.cellsPending(Tall) == cells);

    // Each fill shares its budget between the bodies with cells waiting; together they
    // keep every cell for both, and a fill with nothing waiting does nothing.
    const simple_platformer::FillWork firstStep = simple_platformer::fillNavigation(
        map, cache, simple_platformer::NavigationFillTicksPerStep);
    REQUIRE(firstStep.cells > 0);
    REQUIRE(cache.cellsPending(Small) < cells);
    REQUIRE(cache.cellsPending(Tall) < cells);
    int kept = firstStep.cells;
    for (std::size_t step = 0; step < 2 * cells; ++step)
    {
        const simple_platformer::FillWork work = simple_platformer::fillNavigation(
            map, cache, simple_platformer::NavigationFillTicksPerStep);
        kept += work.cells;
        if (work.cells == 0)
        {
            break;
        }
    }
    REQUIRE(static_cast<std::size_t>(kept) == 2 * cells);
    REQUIRE(cache.size() == 2 * cells);
    REQUIRE(cache.cellsPending(Small) == 0);
    REQUIRE(cache.cellsPending(Tall) == 0);
    REQUIRE(cache.find({2, 1}, Tall) != nullptr);
    REQUIRE(simple_platformer::fillNavigation(map, cache, 1000000).cells == 0);

    REQUIRE_THROWS_AS(simple_platformer::fillNavigation(map, cache, -1), std::invalid_argument);
}
