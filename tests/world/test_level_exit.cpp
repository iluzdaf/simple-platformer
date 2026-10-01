#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/inventory/inventory.hpp"
#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/world/level_exit.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_simulation.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/add_player.hpp"
#include "support/fixed_step.hpp"
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
    tests::player(world).body.bounds.topLeft.x = 60.0F;
    simple_platformer::updateLevelExit(world);
    REQUIRE_FALSE(world.levelComplete());
    tests::player(world).body.bounds.topLeft.x = 16.0F;
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
    tests::player(world).body.bounds.topLeft.x = 80.0F;
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

    // The game pauses while the door opens: the player holds still whatever they intend,
    // and a shot in flight neither moves nor ages.
    simple_platformer::Projectile shot;
    shot.team = simple_platformer::Team::Enemy;
    shot.bounds = {{60.0F, 4.0F}, {2.0F, 2.0F}};
    shot.velocity = {100.0F, 0.0F};
    shot.sprite.size = {2.0F, 2.0F};
    world.addProjectile(shot);
    const auto position = tests::player(world).body.bounds.topLeft;
    for (int tick = 0; tick < 60 && !world.levelComplete(); ++tick)
    {
        tests::player(world).intentions.direction.x = 1.0F;
        simple_platformer::updateWorldSimulation(map, world, tests::FixedStepSeconds);
        REQUIRE(tests::player(world).body.bounds.topLeft == position);
        REQUIRE(world.projectiles().front().bounds.topLeft == shot.bounds.topLeft);
        REQUIRE(world.projectiles().front().lifetimeRemaining == shot.lifetimeRemaining);
    }
    REQUIRE(world.levelComplete());
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

TEST_CASE("World rejects invalid exit data", "[world][exit][validation]")
{
    auto world = makeWorld();
    REQUIRE_THROWS_AS(
        world.setExit(
            exitWith({{0.0F, 0.0F}, {1.0F, 1.0F}}, simple_platformer::ItemStack{3, 0}, false, {})),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        world.setExit(exitWith({{0.0F, 0.0F}, {1.0F, 1.0F}}, {}, false, -1)),
        std::invalid_argument);
}

TEST_CASE("A locked exit records when the living player last stood in it", "[world][exit]")
{
    auto world = makeWorld();
    world.setExit(
        exitWith({{18.0F, 16.0F}, {16.0F, 16.0F}}, simple_platformer::ItemStack{3, 1}, false, 2));
    world.advanceSimulationTime(0.5F);

    tests::player(world).body.bounds.topLeft.x = 80.0F;
    simple_platformer::updateLevelExit(world);
    REQUIRE_FALSE(exitOf(world).lastLockedTouchTimeSeconds.has_value());

    tests::player(world).body.bounds.topLeft.x = 16.0F;
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
