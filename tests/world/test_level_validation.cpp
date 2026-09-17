#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include <stdexcept>

#include <glm/vec2.hpp>

#include "game/example_content.hpp"
#include "game/example_level_catalog.hpp"
#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/world/level_validation.hpp"
#include "simple_platformer/world/world.hpp"

namespace
{
    simple_platformer::Actor makePlatformer(glm::vec2 feet)
    {
        simple_platformer::Actor actor;
        actor.body.bounds.size = {12.0F, 20.0F};
        simple_platformer::placeFeetAt(actor.body.bounds, feet);
        actor.platformerMovement = simple_platformer::PlatformerMovement{};
        return actor;
    }

    simple_platformer::Actor makeFlyer(glm::vec2 feet)
    {
        simple_platformer::Actor actor;
        actor.body.bounds.size = {12.0F, 8.0F};
        simple_platformer::placeFeetAt(actor.body.bounds, feet);
        actor.flyingMovement = simple_platformer::FlyingMovement{};
        return actor;
    }

    void addPatrol(simple_platformer::Actor& actor, glm::vec2 firstFeet, glm::vec2 secondFeet)
    {
        actor.brain = simple_platformer::NpcBrain{};
        actor.senses = simple_platformer::NpcSenses{};
        actor.patrol = simple_platformer::Patrol{firstFeet, secondFeet, true};
        actor.pathFollower = simple_platformer::PathFollower{};
    }
}

TEST_CASE("Level actors require clear spawn positions", "[world][level-validation]")
{
    const auto map = simple_platformer::TileMap::fromAscii({"###", "...", "###"});
    simple_platformer::World world;
    world.addActor(makePlatformer({24.0F, 32.0F}));

    REQUIRE_THROWS_AS(simple_platformer::validateLevelActors(map, world, 1), std::invalid_argument);
}

TEST_CASE("Platformer spawns and patrol points require ground support", "[world][level-validation]")
{
    const auto map = simple_platformer::TileMap::fromAscii({".....", ".....", "#####"});

    SECTION("spawn")
    {
        simple_platformer::World world;
        world.addActor(makePlatformer({24.0F, 16.0F}));
        REQUIRE_THROWS_AS(
            simple_platformer::validateLevelActors(map, world, 1), std::invalid_argument);
    }

    SECTION("patrol point")
    {
        simple_platformer::World world;
        auto actor = makePlatformer({24.0F, 32.0F});
        addPatrol(actor, {16.0F, 32.0F}, {16.0F, 16.0F});
        world.addActor(actor);
        REQUIRE_THROWS_AS(
            simple_platformer::validateLevelActors(map, world, 1), std::invalid_argument);
    }
}

TEST_CASE("The player respawn requires clearance and ground support", "[world][level-validation]")
{
    const auto map = simple_platformer::TileMap::fromAscii({".....", ".....", "#####"});

    SECTION("blocked respawn")
    {
        simple_platformer::World world;
        const auto player = world.addActor(makePlatformer({24.0F, 32.0F}));
        world.setPlayer(player, {24.0F, 48.0F});

        REQUIRE_THROWS_WITH(
            simple_platformer::validateLevelActors(map, world, 7),
            "Level 7 actor 1 respawn overlaps a blocked tile");
    }

    SECTION("unsupported respawn")
    {
        simple_platformer::World world;
        const auto player = world.addActor(makePlatformer({24.0F, 32.0F}));
        world.setPlayer(player, {24.0F, 16.0F});

        REQUIRE_THROWS_WITH(
            simple_platformer::validateLevelActors(map, world, 7),
            "Level 7 actor 1 respawn has no ground support");
    }
}

TEST_CASE("Flying actors require clearance but not ground support", "[world][level-validation]")
{
    const auto map = simple_platformer::TileMap::fromAscii({"...", "...", "###"});
    simple_platformer::World world;
    auto actor = makeFlyer({24.0F, 16.0F});
    addPatrol(actor, {24.0F, 16.0F}, {32.0F, 24.0F});
    world.addActor(actor);

    REQUIRE_NOTHROW(simple_platformer::validateLevelActors(map, world, 1));
}

TEST_CASE("Every level in the example catalog has valid actor placement", "[app][example-content]")
{
    const auto catalog = simple_platformer::loadExampleLevelCatalog();
    for (const simple_platformer::ExampleLevelEntry& entry : catalog.levels)
    {
        auto content = simple_platformer::makeExampleLevel(catalog, entry.number, 0);
        simple_platformer::Actor player = simple_platformer::makeExamplePlayer(0);
        simple_platformer::placeFeetAt(player.body.bounds, content.playerSpawnFeet);
        const auto playerId = content.world.addActor(player);
        content.world.setPlayer(playerId, content.playerSpawnFeet);

        REQUIRE_NOTHROW(
            simple_platformer::validateLevelActors(content.map, content.world, content.number));
    }
}
