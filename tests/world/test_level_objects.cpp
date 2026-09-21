#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>
#include <vector>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/inventory/inventory.hpp"
#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/inventory/item_use.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/render/animation_system.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/render/render_scene.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/level_exit.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"
#include "simple_platformer/world/world_simulation.hpp"
#include "support/require_near.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
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

    simple_platformer::World makeWorld()
    {
        simple_platformer::World world(items());
        const auto id = world.addActor(tests::ActorBuilder::sized({12.0F, 16.0F})
                                           .atFeet({22.0F, 32.0F})
                                           .walking()
                                           .withHealth(1, 3)
                                           .withInventory(simple_platformer::Inventory(2)));
        world.setPlayer(id, {22.0F, 32.0F});
        return world;
    }

    const simple_platformer::LevelExit& exitOf(const simple_platformer::World& world)
    {
        const auto& value = world.exit();
        if (!value.has_value())
        {
            throw std::logic_error("Expected level exit");
        }
        return *value;
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
    "[pickups]")
{
    auto world = makeWorld();
    world.addPickup({{{18.0F, 20.0F}, {8.0F, 8.0F}}, {1, 3}});
    world.addPickup({{{80.0F, 20.0F}, {8.0F, 8.0F}}, {1, 2}});
    simple_platformer::WorldRequests requests;
    simple_platformer::updatePickups(world, requests);
    REQUIRE(tests::inventory(tests::player(world)).count(1) == 0);
    REQUIRE(world.pickups().size() == 2);
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(tests::inventory(tests::player(world)).count(1) == 3);
    REQUIRE(world.pickups().size() == 1);
    REQUIRE(world.pickups().front().bounds.position.x == 80.0F);
    REQUIRE(requests.empty());
}

TEST_CASE("Partial pickups stay in the world and can be collected after freeing space", "[pickups]")
{
    auto world = makeWorld();
    tests::player(world).inventory = simple_platformer::Inventory(1);
    tests::inventory(tests::player(world)).add(world.itemDefinition(1), 4);
    world.addPickup({{{18.0F, 20.0F}, {8.0F, 8.0F}}, {1, 4}});
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
    "[pickups]")
{
    auto world = makeWorld();
    world.addPickup({{{18.0F, 20.0F}, {8.0F, 8.0F}}, {1, 2}});
    world.addPickup({{{18.0F, 20.0F}, {8.0F, 8.0F}}, {2, 1}});
    simple_platformer::WorldRequests requests;
    simple_platformer::updatePickups(world, requests);
    simple_platformer::updatePickups(world, requests);
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(world.pickups().empty());
    REQUIRE(tests::inventory(tests::player(world)).count(1) == 2);
    REQUIRE(tests::inventory(tests::player(world)).count(2) == 1);
}

TEST_CASE("Dead players and players without inventory do not collect pickups", "[pickups]")
{
    auto world = makeWorld();
    world.addPickup({{{18.0F, 20.0F}, {8.0F, 8.0F}}, {1, 2}});
    tests::player(world).life = simple_platformer::LifeState::Dying;
    collect(world);
    REQUIRE(world.pickups().size() == 1);
    tests::player(world).life = simple_platformer::LifeState::Alive;
    tests::player(world).inventory.reset();
    collect(world);
    REQUIRE(world.pickups().size() == 1);
}

TEST_CASE(
    "Potion use consumes one only when healing succeeds and clamps to maximum",
    "[inventory][use]")
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

TEST_CASE("Unusable or stale item requests are harmless", "[inventory][use]")
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

TEST_CASE("An exit checks overlap and its required quantity", "[exit]")
{
    auto world = makeWorld();
    world.setExit(
        {{{18.0F, 16.0F}, {16.0F, 16.0F}}, simple_platformer::ItemStack{1, 2}, false, 2, {}});
    tests::inventory(tests::player(world)).add(world.itemDefinition(1), 1);
    simple_platformer::updateLevelExit(world);
    REQUIRE_FALSE(world.levelComplete());
    tests::inventory(tests::player(world)).add(world.itemDefinition(1), 1);
    tests::player(world).body.bounds.position.x = 60.0F;
    simple_platformer::updateLevelExit(world);
    REQUIRE_FALSE(world.levelComplete());
    tests::player(world).body.bounds.position.x = 16.0F;
    simple_platformer::updateLevelExit(world);
    REQUIRE(world.levelComplete());
    REQUIRE(tests::inventory(tests::player(world)).count(1) == 2);
    REQUIRE(exitOf(world).nextLevel == 2);
}

TEST_CASE("An exit consumes its requirement once and supports final levels", "[exit]")
{
    auto world = makeWorld();
    world.setExit(
        {{{18.0F, 16.0F}, {16.0F, 16.0F}}, simple_platformer::ItemStack{1, 2}, true, {}, {}});
    tests::inventory(tests::player(world)).add(world.itemDefinition(1), 4);
    simple_platformer::updateLevelExit(world);
    simple_platformer::updateLevelExit(world);
    REQUIRE(world.levelComplete());
    REQUIRE(tests::inventory(tests::player(world)).count(1) == 2);
    REQUIRE_FALSE(exitOf(world).nextLevel.has_value());
}

TEST_CASE("Unrestricted exits need no inventory but cannot be used while dying", "[exit]")
{
    auto world = makeWorld();
    world.setExit({{{18.0F, 16.0F}, {16.0F, 16.0F}}, {}, false, {}, {}});
    tests::player(world).inventory.reset();
    tests::player(world).life = simple_platformer::LifeState::Dying;
    simple_platformer::updateLevelExit(world);
    REQUIRE_FALSE(world.levelComplete());
    tests::player(world).life = simple_platformer::LifeState::Alive;
    simple_platformer::updateLevelExit(world);
    REQUIRE(world.levelComplete());
}

TEST_CASE(
    "A simulation tick can collect the key and unlock an overlapping exit",
    "[simulation][exit]")
{
    auto world = makeWorld();
    simple_platformer::TileMap map = tests::TileMapBuilder({"......", "......", "######"});
    world.addPickup({{{18.0F, 20.0F}, {8.0F, 8.0F}}, {3, 1}});
    world.setExit(
        {{{18.0F, 16.0F}, {16.0F, 16.0F}}, simple_platformer::ItemStack{3, 1}, false, 2, {}});
    simple_platformer::updateWorldSimulation(map, world, 1.0F / 60.0F);
    REQUIRE(world.pickups().empty());
    REQUIRE(world.levelComplete());
    const auto position = tests::player(world).body.bounds.position;
    tests::player(world).intentions.direction.x = 1.0F;
    simple_platformer::updateWorldSimulation(map, world, 1.0F / 60.0F);
    REQUIRE(tests::player(world).body.bounds.position == position);
}

TEST_CASE("Respawning preserves the collected inventory", "[inventory][lifecycle]")
{
    auto world = makeWorld();
    tests::inventory(tests::player(world)).add(world.itemDefinition(3), 1);
    tests::player(world).life = simple_platformer::LifeState::Dying;
    world.respawnPlayer();
    REQUIRE(tests::inventory(tests::player(world)).count(3) == 1);
    REQUIRE(tests::player(world).life == simple_platformer::LifeState::Alive);
}

TEST_CASE(
    "Fatal damage prevents collection and exit completion in the same tick",
    "[simulation][pickups]")
{
    auto world = makeWorld();
    tests::player(world).team = simple_platformer::Team::Player;
    world.addPickup({{{18.0F, 20.0F}, {8.0F, 8.0F}}, {3, 1}});
    world.setExit({{{18.0F, 16.0F}, {16.0F, 16.0F}}, {}, false, {}, {}});
    simple_platformer::Projectile projectile;
    projectile.team = simple_platformer::Team::Enemy;
    projectile.bounds = {{20.0F, 20.0F}, {2.0F, 2.0F}};
    projectile.sprite.size = {2.0F, 2.0F};
    world.addProjectile(projectile);
    simple_platformer::TileMap map = tests::TileMapBuilder({"......", "......", "######"});
    simple_platformer::updateWorldSimulation(map, world, 1.0F / 60.0F);
    REQUIRE(tests::player(world).life == simple_platformer::LifeState::Dying);
    REQUIRE(tests::inventory(tests::player(world)).count(3) == 0);
    REQUIRE(world.pickups().size() == 1);
    REQUIRE_FALSE(world.levelComplete());
}

TEST_CASE("Pickups and exits produce camera-relative sprite commands", "[render][pickups]")
{
    auto definitions = items();
    definitions[0].icon = {7, {{4.0F, 8.0F}, {6.0F, 10.0F}}, {6.0F, 10.0F}};
    simple_platformer::World world(definitions);
    world.addPickup({{{20.0F, 20.0F}, {12.0F, 16.0F}}, {1, 1}});
    world.advanceSimulationTime(0.5F);
    simple_platformer::LevelExit exit;
    exit.bounds = {{50.0F, 20.0F}, {16.0F, 32.0F}};
    exit.sprite = simple_platformer::Sprite{8, {{8.0F, 8.0F}, {16.0F, 32.0F}}, {16.0F, 32.0F}};
    world.setExit(exit);
    const simple_platformer::TileMap map = tests::TileMapBuilder({"......", "......", "......"});
    simple_platformer::Camera camera;
    camera.position = {10.0F, 5.0F};
    const auto scene = simple_platformer::buildRenderScene(map, 0, camera, world);
    REQUIRE(scene.sprites.size() == 2);
    REQUIRE(scene.sprites[0].textureId == 7);
    REQUIRE(scene.sprites[0].position.x == 13.0F);
    REQUIRE(scene.sprites[0].position.y == 21.0F);
    REQUIRE(scene.sprites[1].textureId == 8);
    REQUIRE(scene.sprites[1].position.x == 40.0F);
}

TEST_CASE(
    "Pickup sprites use position-based bobbing without moving their bounds",
    "[render][pickups]")
{
    auto definitions = items();
    definitions[0].icon = {7, {{4.0F, 8.0F}, {6.0F, 10.0F}}, {6.0F, 10.0F}};
    simple_platformer::World world(definitions);
    world.addPickup({{{0.0F, 0.0F}, {16.0F, 16.0F}}, {1, 1}});
    world.addPickup({{{16.0F, 0.0F}, {16.0F, 16.0F}}, {1, 1}});
    const simple_platformer::TileMap map = tests::TileMapBuilder({"......", "......", "......"});
    const simple_platformer::Camera camera{{0.0F, 0.0F}, {320.0F, 180.0F}};

    const auto initialScene = simple_platformer::buildRenderScene(map, 0, camera, world);
    world.advanceSimulationTime(0.5F);
    const auto advancedScene = simple_platformer::buildRenderScene(map, 0, camera, world);

    REQUIRE_NEAR(initialScene.sprites[0].position.y, 6.0F);
    REQUIRE_NEAR(initialScene.sprites[1].position.y, 5.0F);
    REQUIRE_NEAR(advancedScene.sprites[0].position.y, 4.0F);
    REQUIRE_NEAR(advancedScene.sprites[1].position.y, 5.0F);
    REQUIRE(world.pickups()[0].bounds.position == glm::vec2{0.0F, 0.0F});
    REQUIRE(world.pickups()[1].bounds.position == glm::vec2{16.0F, 0.0F});
}

TEST_CASE("Pickup sprite overrides leave inventory icons unchanged", "[render][pickups]")
{
    simple_platformer::World world(items());
    const simple_platformer::Sprite sprite{
        7, {{24, 8}, {12, 10}}, {24, 20}, simple_platformer::SpriteAnchor::BodyCenter};
    world.addPickup({{{20, 20}, {8, 8}}, {1, 1}, sprite});
    const simple_platformer::TileMap map = tests::TileMapBuilder({"......", "......", "......"});
    const auto scene = simple_platformer::buildRenderScene(map, 0, {}, world);
    REQUIRE(scene.sprites.size() == 1);
    REQUIRE(scene.sprites[0].textureId == 7);
    REQUIRE(scene.sprites[0].source.position == glm::vec2{24, 8});
    REQUIRE(scene.sprites[0].size == glm::vec2{24, 20});
    REQUIRE(scene.sprites[0].position.x == 12);
    REQUIRE(world.itemDefinition(1).icon.textureId != 7);
    REQUIRE(world.pickups()[0].bounds.size == glm::vec2{8, 8});
}

TEST_CASE("World rejects invalid level object data", "[pickups][exit]")
{
    auto world = makeWorld();
    REQUIRE_THROWS_AS(
        world.addPickup({{{0.0F, 0.0F}, {0.0F, 1.0F}}, {1, 1}}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        world.addPickup({{{0.0F, 0.0F}, {1.0F, 1.0F}}, {99, 1}}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        world.addPickup({{{0.0F, 0.0F}, {1.0F, 1.0F}}, {1, 0}}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        world.setExit(
            {{{0.0F, 0.0F}, {1.0F, 1.0F}}, simple_platformer::ItemStack{3, 0}, false, {}, {}}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        world.setExit({{{0.0F, 0.0F}, {1.0F, 1.0F}}, {}, false, -1, {}}), std::invalid_argument);
    auto definitions = items();
    definitions.push_back(definitions.front());
    REQUIRE_THROWS_AS(simple_platformer::World(definitions), std::invalid_argument);
}
