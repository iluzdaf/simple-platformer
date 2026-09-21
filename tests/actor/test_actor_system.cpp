#include <catch2/catch_test_macros.hpp>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/actor/actor_system.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    simple_platformer::Actor makeActor(glm::vec2 position)
    {
        simple_platformer::Actor actor;
        actor.body.bounds = {position, {12.0F, 12.0F}};
        actor.platformerMovement = simple_platformer::PlatformerMovement{};
        return actor;
    }
}

TEST_CASE("Actor movement consumes its intentions", "[actor][movement]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"..........", "##########"});
    simple_platformer::Actor actor = makeActor({16.0F, 4.0F});
    simple_platformer::PlatformerMovement movement;
    movement.grounded = true;
    actor.platformerMovement = movement;
    actor.intentions.direction.x = 1.0F;
    simple_platformer::World world;
    const simple_platformer::ActorId id = world.addActor(actor);

    simple_platformer::updateActorMovement(map, world, 0.1F);

    const simple_platformer::Actor* moved = world.findActor(id);
    REQUIRE(moved != nullptr);
    REQUIRE(moved->body.bounds.position.x > 16.0F);
    REQUIRE(moved->body.velocity.x > 0.0F);
    REQUIRE(moved->facing == simple_platformer::Facing::Right);
}

TEST_CASE("Dying actors ignore intentions but continue falling", "[actor][movement][lifecycle]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "..........", "##########"});
    simple_platformer::Actor actor = makeActor({16.0F, 4.0F});
    actor.life = simple_platformer::LifeState::Dying;
    actor.intentions.direction.x = 1.0F;
    actor.intentions.jumpPressed = true;
    actor.intentions.jumpHeld = true;
    simple_platformer::World world;
    const simple_platformer::ActorId id = world.addActor(actor);

    simple_platformer::updateActorMovement(map, world, 0.1F);

    const simple_platformer::Actor* moved = world.findActor(id);
    REQUIRE(moved != nullptr);
    REQUIRE(moved->body.bounds.position.x == 16.0F);
    REQUIRE(moved->body.velocity.x == 0.0F);
    REQUIRE(moved->body.bounds.position.y > 4.0F);
    REQUIRE(moved->body.velocity.y > 0.0F);
}

TEST_CASE("Aim direction controls horizontal facing independently of movement", "[actor][movement]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"..........", "##########"});
    simple_platformer::Actor actor = makeActor({16.0F, 4.0F});
    simple_platformer::PlatformerMovement movement;
    movement.grounded = true;
    actor.platformerMovement = movement;
    actor.intentions.direction.x = 1.0F;
    actor.intentions.aimDirection = {-1.0F, -1.0F};
    simple_platformer::World world;
    const simple_platformer::ActorId id = world.addActor(actor);

    simple_platformer::updateActorMovement(map, world, 0.1F);

    const simple_platformer::Actor* moved = world.findActor(id);
    REQUIRE(moved != nullptr);
    REQUIRE(moved->body.velocity.x > 0.0F);
    REQUIRE(moved->facing == simple_platformer::Facing::Left);
}
