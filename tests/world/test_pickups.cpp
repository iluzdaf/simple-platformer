#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/inventory/inventory.hpp"
#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/add_player.hpp"
#include "support/require_near.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    std::vector<simple_platformer::ItemDefinition> items()
    {
        return {
            {1, "Coin", {}, 5},
            {2, "Potion", {}, 5, simple_platformer::ItemEffect::Heal, 2},
            {3, "Key", {}, 1}};
    }

    simple_platformer::Pickup pickupAt(
        glm::vec2 position,
        glm::vec2 size,
        simple_platformer::ItemStack stack)
    {
        simple_platformer::Pickup pickup;
        pickup.body.bounds = {position, size};
        pickup.stack = stack;
        return pickup;
    }

    simple_platformer::World makeWorld()
    {
        simple_platformer::World world(items());
        tests::addPlayer(
            world,
            tests::ActorBuilder::sized({12.0F, 16.0F})
                .atFeet({22.0F, 32.0F})
                .platforming()
                .withHealth(1, 3)
                .withInventory(simple_platformer::Inventory(2)));
        return world;
    }

    void collect(simple_platformer::World& world)
    {
        simple_platformer::WorldRequests requests;
        simple_platformer::updatePickups(world, requests);
        simple_platformer::applyWorldRequests(world, requests);
        REQUIRE(requests.empty());
    }
}

TEST_CASE(
    "Automatic pickups collect overlapping items only after requests are applied",
    "[world][pickups]")
{
    auto world = makeWorld();
    world.addPickup(pickupAt({18.0F, 20.0F}, {8.0F, 8.0F}, {1, 3}));
    world.addPickup(pickupAt({80.0F, 20.0F}, {8.0F, 8.0F}, {1, 2}));
    simple_platformer::WorldRequests requests;
    simple_platformer::updatePickups(world, requests);
    REQUIRE(tests::inventory(tests::player(world)).count(1) == 0);
    REQUIRE(world.pickups().size() == 2);
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(tests::inventory(tests::player(world)).count(1) == 3);
    REQUIRE(world.pickups().size() == 1);
    REQUIRE(world.pickups().front().body.bounds.topLeft.x == 80.0F);
    REQUIRE(requests.empty());
}

TEST_CASE(
    "Partial pickups stay in the world and can be collected after freeing space",
    "[world][pickups]")
{
    auto world = makeWorld();
    tests::player(world).inventory = simple_platformer::Inventory(1);
    tests::inventory(tests::player(world)).add(world.itemDefinition(1), 4);
    world.addPickup(pickupAt({18.0F, 20.0F}, {8.0F, 8.0F}, {1, 4}));
    collect(world);
    REQUIRE(tests::inventory(tests::player(world)).count(1) == 5);
    REQUIRE(world.pickups().front().stack.quantity == 3);
    collect(world);
    REQUIRE(world.pickups().front().stack.quantity == 3);
    tests::inventory(tests::player(world)).remove(1, 3);
    collect(world);
    REQUIRE(world.pickups().empty());
    REQUIRE(tests::inventory(tests::player(world)).count(1) == 5);
}

TEST_CASE(
    "Multiple overlapping pickups and duplicate requests do not skip or duplicate items",
    "[world][pickups]")
{
    auto world = makeWorld();
    world.addPickup(pickupAt({18.0F, 20.0F}, {8.0F, 8.0F}, {1, 2}));
    world.addPickup(pickupAt({18.0F, 20.0F}, {8.0F, 8.0F}, {2, 1}));
    simple_platformer::WorldRequests requests;
    simple_platformer::updatePickups(world, requests);
    simple_platformer::updatePickups(world, requests);
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(world.pickups().empty());
    REQUIRE(tests::inventory(tests::player(world)).count(1) == 2);
    REQUIRE(tests::inventory(tests::player(world)).count(2) == 1);
}

TEST_CASE("Dead players and players without inventory do not collect pickups", "[world][pickups]")
{
    auto world = makeWorld();
    world.addPickup(pickupAt({18.0F, 20.0F}, {8.0F, 8.0F}, {1, 2}));
    tests::player(world).life = simple_platformer::LifeState::Dying;
    collect(world);
    REQUIRE(world.pickups().size() == 1);
    tests::player(world).life = simple_platformer::LifeState::Alive;
    tests::player(world).inventory.reset();
    collect(world);
    REQUIRE(world.pickups().size() == 1);
}

TEST_CASE("World rejects invalid pickup data", "[world][pickups][validation]")
{
    auto world = makeWorld();
    REQUIRE_THROWS_AS(
        world.addPickup(pickupAt({0.0F, 0.0F}, {0.0F, 1.0F}, {1, 1})), std::invalid_argument);
    REQUIRE_THROWS_AS(
        world.addPickup(pickupAt({0.0F, 0.0F}, {1.0F, 1.0F}, {99, 1})), std::invalid_argument);
    REQUIRE_THROWS_AS(
        world.addPickup(pickupAt({0.0F, 0.0F}, {1.0F, 1.0F}, {1, 0})), std::invalid_argument);
}

TEST_CASE("A pickup falls until it rests on a tile", "[world][pickups]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", "....", "####"});
    simple_platformer::World world(items());
    world.addPickup(pickupAt({4.0F, 4.0F}, {8.0F, 8.0F}, {1, 1}));
    const simple_platformer::Pickup& pickup = world.pickups().front();

    simple_platformer::updatePickupMovement(map, world, 0.1F);

    REQUIRE_NEAR(pickup.body.velocity.y, simple_platformer::DefaultGravity * 0.1F);
    REQUIRE_NEAR(pickup.body.bounds.topLeft.y, 4.0F + pickup.body.velocity.y * 0.1F);

    for (int step = 0; step < 10; ++step)
    {
        simple_platformer::updatePickupMovement(map, world, 0.1F);
    }

    // Resting on the floor, whose top edge is two tiles down.
    REQUIRE_NEAR(pickup.body.bounds.topLeft.y, 24.0F);
    REQUIRE_NEAR(pickup.body.velocity.y, 0.0F);
    REQUIRE_NEAR(pickup.body.bounds.topLeft.x, 4.0F);
}

TEST_CASE("A pickup falls through the tile that breaks beneath it", "[world][pickups]")
{
    simple_platformer::TileMap map =
        tests::TileMapBuilder({"....", "XXXX", "....", "####"})
            .where('X', tests::Tile().blocksMovement().breaksInto('.'));
    simple_platformer::World world(items());
    world.addPickup(pickupAt({4.0F, 8.0F}, {8.0F, 8.0F}, {1, 1}));
    const simple_platformer::Pickup& pickup = world.pickups().front();

    simple_platformer::updatePickupMovement(map, world, 0.1F);
    REQUIRE_NEAR(pickup.body.bounds.topLeft.y, 8.0F);

    REQUIRE(map.breakTile({0, 1}));
    for (int step = 0; step < 12; ++step)
    {
        simple_platformer::updatePickupMovement(map, world, 0.1F);
    }

    REQUIRE_NEAR(pickup.body.bounds.topLeft.y, 40.0F);
    REQUIRE_NEAR(pickup.body.velocity.y, 0.0F);
}

TEST_CASE("Pickup movement rejects a negative step", "[world][pickups]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"..", "##"});
    simple_platformer::World world(items());
    REQUIRE_THROWS_AS(
        simple_platformer::updatePickupMovement(map, world, -0.1F), std::invalid_argument);
}
