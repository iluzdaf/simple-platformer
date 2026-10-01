#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <vector>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/inventory/inventory.hpp"
#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/inventory/item_use.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/add_player.hpp"

namespace
{
    std::vector<simple_platformer::ItemDefinition> items()
    {
        return {
            {1, "Coin", {}, 5},
            {2, "Potion", {}, 5, simple_platformer::ItemEffect::Heal, 2},
            {3, "Key", {}, 1}};
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
}

TEST_CASE(
    "Potion use consumes one only when healing succeeds and clamps to maximum",
    "[world][inventory][use]")
{
    auto world = makeWorld();
    tests::inventory(tests::player(world)).add(world.itemDefinition(2), 3);
    simple_platformer::WorldRequests requests;
    requests.useItem(world.playerId(), 0);
    REQUIRE(tests::health(tests::player(world)).current == 1);
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(tests::health(tests::player(world)).current == 3);
    REQUIRE(tests::inventory(tests::player(world)).count(2) == 2);
    REQUIRE(requests.empty());
    REQUIRE_FALSE(simple_platformer::useItem(world, world.playerId(), 0));
    REQUIRE(tests::inventory(tests::player(world)).count(2) == 2);
    tests::health(tests::player(world)).current = 2;
    REQUIRE(simple_platformer::useItem(world, world.playerId(), 0));
    REQUIRE(tests::health(tests::player(world)).current == 3);
    REQUIRE(tests::inventory(tests::player(world)).count(2) == 1);
}

TEST_CASE("Unusable or stale item requests are harmless", "[world][inventory][use]")
{
    auto world = makeWorld();
    tests::inventory(tests::player(world)).add(world.itemDefinition(1), 1);
    tests::inventory(tests::player(world)).add(world.itemDefinition(2), 1);
    REQUIRE_FALSE(simple_platformer::useItem(world, world.playerId(), 0));
    REQUIRE_FALSE(simple_platformer::useItem(world, world.playerId(), 99));
    REQUIRE_FALSE(simple_platformer::useItem(world, simple_platformer::ActorId{999}, 0));
    tests::player(world).life = simple_platformer::LifeState::Dying;
    REQUIRE_FALSE(simple_platformer::useItem(world, world.playerId(), 1));
    tests::player(world).life = simple_platformer::LifeState::Alive;
    tests::player(world).health.reset();
    REQUIRE_FALSE(simple_platformer::useItem(world, world.playerId(), 1));
    REQUIRE(tests::inventory(tests::player(world)).count(2) == 1);
}

TEST_CASE("Respawning preserves the collected inventory", "[world][inventory][lifecycle]")
{
    auto world = makeWorld();
    tests::inventory(tests::player(world)).add(world.itemDefinition(3), 1);
    tests::player(world).life = simple_platformer::LifeState::Dying;
    world.respawnPlayer();
    REQUIRE(tests::inventory(tests::player(world)).count(3) == 1);
    REQUIRE(tests::player(world).life == simple_platformer::LifeState::Alive);
}

TEST_CASE("World rejects duplicate item definitions", "[world][inventory][validation]")
{
    auto definitions = items();
    definitions.push_back(definitions.front());
    REQUIRE_THROWS_AS(simple_platformer::World(definitions), std::invalid_argument);
}
