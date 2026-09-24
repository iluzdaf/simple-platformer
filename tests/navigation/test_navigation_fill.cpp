#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <stdexcept>

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

TEST_CASE(
    "Queued navigation keeps every cell for each walking NPC body over the fills",
    "[navigation][fill]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
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
    const std::size_t cells =
        static_cast<std::size_t>(map.width()) * static_cast<std::size_t>(map.height());
    const simple_platformer::ConnectionBody tall{
        {12.0F, 20.0F}, simple_platformer::PlatformerMovementConfig{}, tests::FixedStepSeconds};
    const simple_platformer::ConnectionBody small{
        {12.0F, 12.0F}, simple_platformer::PlatformerMovementConfig{}, tests::FixedStepSeconds};

    // Queuing keeps nothing yet.
    simple_platformer::queueWorldNavigation(map, world, tests::FixedStepSeconds);
    REQUIRE(world.platformerConnections().size() == 0);
    REQUIRE(world.platformerConnections().cellsPending(tall) == cells);
    REQUIRE(world.platformerConnections().cellsPending(small) == cells);

    // Each fill shares the step's budget between the bodies; together they keep every
    // cell for both, and a fill with nothing waiting does nothing.
    const simple_platformer::FillWork firstStep =
        simple_platformer::fillWorldNavigation(map, world, tests::FixedStepSeconds);
    REQUIRE(firstStep.cells > 0);
    REQUIRE(world.platformerConnections().cellsPending(tall) < cells);
    REQUIRE(world.platformerConnections().cellsPending(small) < cells);
    int kept = firstStep.cells;
    for (std::size_t step = 0; step < 2 * cells; ++step)
    {
        const simple_platformer::FillWork work =
            simple_platformer::fillWorldNavigation(map, world, tests::FixedStepSeconds);
        kept += work.cells;
        if (work.cells == 0)
        {
            break;
        }
    }
    REQUIRE(static_cast<std::size_t>(kept) == 2 * cells);
    REQUIRE(world.platformerConnections().size() == 2 * cells);
    REQUIRE(world.platformerConnections().cellsPending(tall) == 0);
    REQUIRE(world.platformerConnections().cellsPending(small) == 0);
    REQUIRE(world.platformerConnections().find({2, 1}, tall) != nullptr);
    REQUIRE(simple_platformer::fillWorldNavigation(map, world, tests::FixedStepSeconds).cells == 0);

    REQUIRE_THROWS_AS(
        simple_platformer::queueWorldNavigation(map, world, 0.0F), std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::fillWorldNavigation(map, world, 0.0F), std::invalid_argument);
}
