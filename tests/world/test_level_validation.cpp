#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include <glm/vec2.hpp>

#include "game/example_content.hpp"
#include "game/example_items.hpp"
#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/world/level_validation.hpp"
#include "simple_platformer/world/tile_map.hpp"
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

TEST_CASE("Flying actors require clearance but not ground support", "[world][level-validation]")
{
    const auto map = simple_platformer::TileMap::fromAscii({"...", "...", "###"});
    simple_platformer::World world;
    auto actor = makeFlyer({24.0F, 16.0F});
    addPatrol(actor, {24.0F, 16.0F}, {32.0F, 24.0F});
    world.addActor(actor);

    REQUIRE_NOTHROW(simple_platformer::validateLevelActors(map, world, 1));
}

TEST_CASE("The supplied example levels have valid actor placement", "[app][example-content]")
{
    for (const int level : {1, 2})
    {
        const auto map = simple_platformer::makeExampleLevel(level);
        simple_platformer::World world(simple_platformer::makeExampleItems(0));
        const auto player = world.addActor(simple_platformer::makeExamplePlayer(0));
        world.setPlayer(player, {38.0F, 208.0F});
        simple_platformer::populateExampleLevel(world, level, 0);

        REQUIRE_NOTHROW(simple_platformer::validateLevelActors(map, world, level));
    }
}
