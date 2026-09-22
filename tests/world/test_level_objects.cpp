#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/inventory/inventory.hpp"
#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/inventory/item_use.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/physics/body.hpp"
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
#include "support/add_player.hpp"
#include "support/fixed_step.hpp"

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
                .walking()
                .withHealth(1, 3)
                .withInventory(simple_platformer::Inventory(2)));
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

    simple_platformer::LevelExit exitWith(
        simple_platformer::Aabb bounds,
        std::optional<simple_platformer::ItemStack> requirement,
        bool consumeItem,
        std::optional<int> nextLevel)
    {
        simple_platformer::LevelExit exit;
        exit.bounds = bounds;
        exit.requirement = requirement;
        exit.consumeItem = consumeItem;
        exit.nextLevel = nextLevel;
        return exit;
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
    REQUIRE(world.pickups().front().body.bounds.position.x == 80.0F);
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

TEST_CASE("An exit checks overlap and its required quantity", "[world][exit]")
{
    auto world = makeWorld();
    world.setExit(
        exitWith({{18.0F, 16.0F}, {16.0F, 16.0F}}, simple_platformer::ItemStack{1, 2}, false, 2));
    tests::inventory(tests::player(world)).add(world.itemDefinition(1), 1);
    simple_platformer::updateLevelExit(world);
    REQUIRE_FALSE(world.levelComplete());
    tests::inventory(tests::player(world)).add(world.itemDefinition(1), 1);
    tests::player(world).body.bounds.position.x = 60.0F;
    simple_platformer::updateLevelExit(world);
    REQUIRE_FALSE(world.levelComplete());
    tests::player(world).body.bounds.position.x = 16.0F;
    simple_platformer::updateLevelExit(world);
    REQUIRE(simple_platformer::exitOpening(world));
    REQUIRE_FALSE(world.levelComplete());
    world.advanceSimulationTime(simple_platformer::ExitOpenSeconds);
    simple_platformer::updateLevelExit(world);
    REQUIRE(world.levelComplete());
    REQUIRE_FALSE(simple_platformer::exitOpening(world));
    REQUIRE(tests::inventory(tests::player(world)).count(1) == 2);
    REQUIRE(exitOf(world).nextLevel == 2);
}

TEST_CASE("An entered exit completes only once it has had time to open", "[world][exit]")
{
    auto world = makeWorld();
    world.setExit(exitWith({{18.0F, 16.0F}, {16.0F, 16.0F}}, {}, false, 2));
    world.advanceSimulationTime(0.5F);
    simple_platformer::updateLevelExit(world);
    REQUIRE(exitOf(world).openedTimeSeconds == 0.5F);

    // Leaving the doorway afterwards changes nothing; the door is already opening.
    tests::player(world).body.bounds.position.x = 80.0F;
    world.advanceSimulationTime(simple_platformer::ExitOpenSeconds * 0.5F);
    simple_platformer::updateLevelExit(world);
    REQUIRE_FALSE(world.levelComplete());

    world.advanceSimulationTime(simple_platformer::ExitOpenSeconds * 0.5F);
    simple_platformer::updateLevelExit(world);
    REQUIRE(world.levelComplete());
}

TEST_CASE("An exit consumes its requirement once and supports final levels", "[world][exit]")
{
    auto world = makeWorld();
    world.setExit(
        exitWith({{18.0F, 16.0F}, {16.0F, 16.0F}}, simple_platformer::ItemStack{1, 2}, true, {}));
    tests::inventory(tests::player(world)).add(world.itemDefinition(1), 4);
    simple_platformer::updateLevelExit(world);
    // The requirement goes as the door starts opening, and is not asked for again.
    REQUIRE(tests::inventory(tests::player(world)).count(1) == 2);
    simple_platformer::updateLevelExit(world);
    REQUIRE(tests::inventory(tests::player(world)).count(1) == 2);
    REQUIRE_FALSE(world.levelComplete());
    world.advanceSimulationTime(simple_platformer::ExitOpenSeconds);
    simple_platformer::updateLevelExit(world);
    REQUIRE(world.levelComplete());
    REQUIRE(tests::inventory(tests::player(world)).count(1) == 2);
    REQUIRE_FALSE(exitOf(world).nextLevel.has_value());
}

TEST_CASE("Unrestricted exits need no inventory but cannot be used while dying", "[world][exit]")
{
    auto world = makeWorld();
    world.setExit(exitWith({{18.0F, 16.0F}, {16.0F, 16.0F}}, {}, false, {}));
    tests::player(world).inventory.reset();
    tests::player(world).life = simple_platformer::LifeState::Dying;
    simple_platformer::updateLevelExit(world);
    REQUIRE_FALSE(simple_platformer::exitOpening(world));
    tests::player(world).life = simple_platformer::LifeState::Alive;
    simple_platformer::updateLevelExit(world);
    REQUIRE(simple_platformer::exitOpening(world));
    world.advanceSimulationTime(simple_platformer::ExitOpenSeconds);
    simple_platformer::updateLevelExit(world);
    REQUIRE(world.levelComplete());
}

TEST_CASE(
    "A simulation tick can collect the key and unlock an overlapping exit",
    "[world][simulation][exit]")
{
    auto world = makeWorld();
    simple_platformer::TileMap map = tests::TileMapBuilder({"......", "......", "######"});
    world.addPickup(pickupAt({18.0F, 20.0F}, {8.0F, 8.0F}, {3, 1}));
    world.setExit(
        exitWith({{18.0F, 16.0F}, {16.0F, 16.0F}}, simple_platformer::ItemStack{3, 1}, false, 2));
    simple_platformer::updateWorldSimulation(map, world, tests::FixedStepSeconds);
    REQUIRE(world.pickups().empty());
    REQUIRE(simple_platformer::exitOpening(world));
    REQUIRE_FALSE(world.levelComplete());

    // The player holds still in the doorway while it opens, whatever they intend.
    const auto position = tests::player(world).body.bounds.position;
    tests::player(world).intentions.direction.x = 1.0F;
    simple_platformer::updateWorldSimulation(map, world, tests::FixedStepSeconds);
    REQUIRE(tests::player(world).body.bounds.position == position);

    for (int tick = 0; tick < 60 && !world.levelComplete(); ++tick)
    {
        tests::player(world).intentions.direction.x = 1.0F;
        simple_platformer::updateWorldSimulation(map, world, tests::FixedStepSeconds);
    }
    REQUIRE(world.levelComplete());
    REQUIRE(tests::player(world).body.bounds.position == position);
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

TEST_CASE(
    "Fatal damage prevents collection and exit completion in the same tick",
    "[world][simulation][pickups]")
{
    auto world = makeWorld();
    tests::player(world).team = simple_platformer::Team::Player;
    world.addPickup(pickupAt({18.0F, 20.0F}, {8.0F, 8.0F}, {3, 1}));
    world.setExit(exitWith({{18.0F, 16.0F}, {16.0F, 16.0F}}, {}, false, {}));
    simple_platformer::Projectile projectile;
    projectile.team = simple_platformer::Team::Enemy;
    projectile.bounds = {{20.0F, 20.0F}, {2.0F, 2.0F}};
    projectile.sprite.size = {2.0F, 2.0F};
    world.addProjectile(projectile);
    simple_platformer::TileMap map = tests::TileMapBuilder({"......", "......", "######"});
    simple_platformer::updateWorldSimulation(map, world, tests::FixedStepSeconds);
    REQUIRE(tests::player(world).life == simple_platformer::LifeState::Dying);
    REQUIRE(tests::inventory(tests::player(world)).count(3) == 0);
    REQUIRE(world.pickups().size() == 1);
    REQUIRE_FALSE(world.levelComplete());
}

TEST_CASE("Pickups and exits produce camera-relative sprite commands", "[world][render][pickups]")
{
    auto definitions = items();
    definitions[0].icon = {7, {{4.0F, 8.0F}, {6.0F, 10.0F}}, {6.0F, 10.0F}};
    simple_platformer::World world(definitions);
    world.addPickup(pickupAt({20.0F, 20.0F}, {12.0F, 16.0F}, {1, 1}));
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
    "[world][render][pickups]")
{
    auto definitions = items();
    definitions[0].icon = {7, {{4.0F, 8.0F}, {6.0F, 10.0F}}, {6.0F, 10.0F}};
    simple_platformer::World world(definitions);
    world.addPickup(pickupAt({0.0F, 0.0F}, {16.0F, 16.0F}, {1, 1}));
    world.addPickup(pickupAt({16.0F, 0.0F}, {16.0F, 16.0F}, {1, 1}));
    const simple_platformer::TileMap map = tests::TileMapBuilder({"......", "......", "......"});
    const simple_platformer::Camera camera{{0.0F, 0.0F}, simple_platformer::InternalViewportSize};

    const auto initialScene = simple_platformer::buildRenderScene(map, 0, camera, world);
    world.advanceSimulationTime(0.5F);
    const auto advancedScene = simple_platformer::buildRenderScene(map, 0, camera, world);

    REQUIRE_NEAR(initialScene.sprites[0].position.y, 6.0F);
    REQUIRE_NEAR(initialScene.sprites[1].position.y, 5.0F);
    REQUIRE_NEAR(advancedScene.sprites[0].position.y, 4.0F);
    REQUIRE_NEAR(advancedScene.sprites[1].position.y, 5.0F);
    REQUIRE(world.pickups()[0].body.bounds.position == glm::vec2{0.0F, 0.0F});
    REQUIRE(world.pickups()[1].body.bounds.position == glm::vec2{16.0F, 0.0F});
}

TEST_CASE("Pickup sprite overrides leave inventory icons unchanged", "[world][render][pickups]")
{
    simple_platformer::World world(items());
    const simple_platformer::Sprite sprite{
        7, {{24, 8}, {12, 10}}, {24, 20}, simple_platformer::SpriteAnchor::BodyCenter};
    simple_platformer::Pickup pickup = pickupAt({20.0F, 20.0F}, {8.0F, 8.0F}, {1, 1});
    pickup.sprite = sprite;
    world.addPickup(pickup);
    const simple_platformer::TileMap map = tests::TileMapBuilder({"......", "......", "......"});
    const auto scene = simple_platformer::buildRenderScene(map, 0, {}, world);
    REQUIRE(scene.sprites.size() == 1);
    REQUIRE(scene.sprites[0].textureId == 7);
    REQUIRE(scene.sprites[0].source.position == glm::vec2{24, 8});
    REQUIRE(scene.sprites[0].size == glm::vec2{24, 20});
    REQUIRE(scene.sprites[0].position.x == 12);
    REQUIRE(world.itemDefinition(1).icon.textureId != 7);
    REQUIRE(world.pickups()[0].body.bounds.size == glm::vec2{8, 8});
}

TEST_CASE("World rejects invalid level object data", "[world][pickups][exit]")
{
    auto world = makeWorld();
    REQUIRE_THROWS_AS(
        world.addPickup(pickupAt({0.0F, 0.0F}, {0.0F, 1.0F}, {1, 1})), std::invalid_argument);
    REQUIRE_THROWS_AS(
        world.addPickup(pickupAt({0.0F, 0.0F}, {1.0F, 1.0F}, {99, 1})), std::invalid_argument);
    REQUIRE_THROWS_AS(
        world.addPickup(pickupAt({0.0F, 0.0F}, {1.0F, 1.0F}, {1, 0})), std::invalid_argument);
    REQUIRE_THROWS_AS(
        world.setExit(
            exitWith({{0.0F, 0.0F}, {1.0F, 1.0F}}, simple_platformer::ItemStack{3, 0}, false, {})),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        world.setExit(exitWith({{0.0F, 0.0F}, {1.0F, 1.0F}}, {}, false, -1)),
        std::invalid_argument);
    auto definitions = items();
    definitions.push_back(definitions.front());
    REQUIRE_THROWS_AS(simple_platformer::World(definitions), std::invalid_argument);
}

TEST_CASE("A pickup falls until it rests on a tile", "[world][pickups]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", "....", "####"});
    simple_platformer::World world(items());
    world.addPickup(pickupAt({4.0F, 4.0F}, {8.0F, 8.0F}, {1, 1}));
    const simple_platformer::Pickup& pickup = world.pickups().front();

    simple_platformer::updatePickupMovement(map, world, 0.1F);

    REQUIRE_NEAR(pickup.body.velocity.y, simple_platformer::DefaultGravity * 0.1F);
    REQUIRE_NEAR(pickup.body.bounds.position.y, 4.0F + pickup.body.velocity.y * 0.1F);

    for (int step = 0; step < 10; ++step)
    {
        simple_platformer::updatePickupMovement(map, world, 0.1F);
    }

    // Resting on the floor, whose top edge is two tiles down.
    REQUIRE_NEAR(pickup.body.bounds.position.y, 24.0F);
    REQUIRE_NEAR(pickup.body.velocity.y, 0.0F);
    REQUIRE_NEAR(pickup.body.bounds.position.x, 4.0F);
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
    REQUIRE_NEAR(pickup.body.bounds.position.y, 8.0F);

    REQUIRE(map.breakTile({0, 1}));
    for (int step = 0; step < 12; ++step)
    {
        simple_platformer::updatePickupMovement(map, world, 0.1F);
    }

    REQUIRE_NEAR(pickup.body.bounds.position.y, 40.0F);
    REQUIRE_NEAR(pickup.body.velocity.y, 0.0F);
}

TEST_CASE("A locked exit records when the living player last stood in it", "[world][exit]")
{
    auto world = makeWorld();
    world.setExit(
        exitWith({{18.0F, 16.0F}, {16.0F, 16.0F}}, simple_platformer::ItemStack{3, 1}, false, 2));
    world.advanceSimulationTime(0.5F);

    tests::player(world).body.bounds.position.x = 80.0F;
    simple_platformer::updateLevelExit(world);
    REQUIRE_FALSE(exitOf(world).lastLockedTouchTimeSeconds.has_value());

    tests::player(world).body.bounds.position.x = 16.0F;
    simple_platformer::updateLevelExit(world);
    REQUIRE(exitOf(world).lastLockedTouchTimeSeconds == 0.5F);
    REQUIRE_FALSE(world.levelComplete());

    world.advanceSimulationTime(0.25F);
    tests::player(world).life = simple_platformer::LifeState::Dying;
    simple_platformer::updateLevelExit(world);
    REQUIRE(exitOf(world).lastLockedTouchTimeSeconds == 0.5F);

    tests::player(world).life = simple_platformer::LifeState::Alive;
    simple_platformer::updateLevelExit(world);
    REQUIRE(exitOf(world).lastLockedTouchTimeSeconds == 0.75F);

    // Meeting the requirement opens the exit and is not a locked touch.
    tests::inventory(tests::player(world)).add(world.itemDefinition(3), 1);
    world.advanceSimulationTime(0.25F);
    simple_platformer::updateLevelExit(world);
    REQUIRE(simple_platformer::exitOpening(world));
    REQUIRE(exitOf(world).lastLockedTouchTimeSeconds == 0.75F);
}

TEST_CASE("World rejects an exit touched or opened outside simulation time", "[world][exit]")
{
    auto world = makeWorld();
    simple_platformer::LevelExit exit =
        exitWith({{18.0F, 16.0F}, {16.0F, 16.0F}}, simple_platformer::ItemStack{3, 1}, false, 2);
    exit.lastLockedTouchTimeSeconds = 1.0F;
    REQUIRE_THROWS_AS(world.setExit(exit), std::invalid_argument);
    exit.lastLockedTouchTimeSeconds = -1.0F;
    REQUIRE_THROWS_AS(world.setExit(exit), std::invalid_argument);
    exit.lastLockedTouchTimeSeconds.reset();
    exit.openedTimeSeconds = 1.0F;
    REQUIRE_THROWS_AS(world.setExit(exit), std::invalid_argument);

    world.advanceSimulationTime(1.0F);
    exit.lastLockedTouchTimeSeconds = 1.0F;
    REQUIRE_NOTHROW(world.setExit(exit));
}
