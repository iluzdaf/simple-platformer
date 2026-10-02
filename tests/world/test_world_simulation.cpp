#include <catch2/catch_message.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/catch_test_macros.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_simulation.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/tile_map_builder.hpp"
#include "support/fixed_step.hpp"

TEST_CASE("World simulation advances its shared clock once per update", "[world][simulation][time]")
{
    simple_platformer::TileMap map = tests::TileMapBuilder({"."});
    simple_platformer::World world;

    simple_platformer::updateWorldSimulation(map, world, 0.25F);
    simple_platformer::updateWorldSimulation(map, world, 0.25F);

    REQUIRE(world.simulationTimeSeconds() == 0.5F);

    world.completeLevel();
    simple_platformer::updateWorldSimulation(map, world, 0.25F);

    REQUIRE(world.simulationTimeSeconds() == 0.5F);
}

TEST_CASE("World simulation spawns a projectile after projectile movement", "[world][simulation]")
{
    simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::World world;
    simple_platformer::Actor player = tests::ActorBuilder::sized({12.0F, 12.0F})
                                          .atFeet({22.0F, 28.0F})
                                          .platforming()
                                          .onTeam(simple_platformer::Team::Player)
                                          .shooting();
    player.intentions.aimDirection = {1.0F, 0.0F};
    player.intentions.primaryAttackPressed = true;
    const simple_platformer::ActorId playerId = world.addActor(player);

    simple_platformer::updateWorldSimulation(map, world, tests::FixedStepSeconds);

    REQUIRE(world.projectiles().size() == 1);
    const float spawnPosition = world.projectiles().front().bounds.topLeft.x;

    simple_platformer::Actor& storedPlayer = tests::actor(world, playerId);
    storedPlayer.intentions.primaryAttackPressed = false;
    simple_platformer::updateWorldSimulation(map, world, tests::FixedStepSeconds);

    REQUIRE(world.projectiles().size() == 1);
    REQUIRE(world.projectiles().front().bounds.topLeft.x > spawnPosition);
}

TEST_CASE("World simulation lets a pickup fall onto the tile below", "[world][simulation]")
{
    simple_platformer::TileMap map = tests::TileMapBuilder({"....", "....", "####"});
    simple_platformer::World world({{1, "Coin", {}, 5}});
    simple_platformer::Pickup pickup;
    pickup.body.bounds = {{4.0F, 4.0F}, {8.0F, 8.0F}};
    pickup.stack = {1, 1};
    world.addPickup(pickup);

    for (int step = 0; step < 12; ++step)
    {
        simple_platformer::updateWorldSimulation(map, world, 0.1F);
    }

    REQUIRE(world.pickups().front().body.bounds.topLeft.y == 24.0F);
}
