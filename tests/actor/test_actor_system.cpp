#include <catch2/catch_test_macros.hpp>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/actor/actor_system.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    simple_platformer::Actor makeActor(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F}).atFeet(feet).walking();
    }
}

TEST_CASE("Actor movement consumes its intentions", "[actor][movement]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"..........", "##########"});
    simple_platformer::Actor actor = makeActor({22.0F, 16.0F});
    tests::platformerMovement(actor).grounded = true;
    actor.intentions.direction.x = 1.0F;
    simple_platformer::World world;
    const simple_platformer::ActorId id = world.addActor(actor);

    simple_platformer::updateActorMovement(map, world, 0.1F);

    simple_platformer::Actor& moved = tests::actor(world, id);
    REQUIRE(moved.body.bounds.topLeft.x > 16.0F);
    REQUIRE(moved.body.velocity.x > 0.0F);
    REQUIRE(moved.facing == simple_platformer::Facing::Right);
}

TEST_CASE("An actor's optional climb component uses its climb request", "[actor][movement]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"......", "..c...", "..c...", "..c...", "######"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    simple_platformer::Actor climber =
        tests::ActorBuilder::sized({12.0F, 12.0F}).atFeet({54.0F, 48.0F}).walking().climbing();
    climber.intentions.climbGrip = simple_platformer::ClimbGrip::Hold;
    climber.intentions.direction.y = -1.0F;
    simple_platformer::World world;
    const auto id = world.addActor(climber);

    simple_platformer::updateActorMovement(map, world, 0.1F);

    const auto& moved = tests::actor(world, id);
    REQUIRE(
        tests::surfaceClimb(tests::actor(world, id)).surface ==
        simple_platformer::ClimbSurface::LeftWall);
    REQUIRE(moved.body.bounds.topLeft.y < 36.0F);
}

TEST_CASE("Dying actors ignore intentions but continue falling", "[actor][movement][lifecycle]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "..........", "##########"});
    simple_platformer::Actor actor = makeActor({22.0F, 16.0F});
    actor.life = simple_platformer::LifeState::Dying;
    actor.intentions.direction.x = 1.0F;
    actor.intentions.jumpPressed = true;
    actor.intentions.jumpHeld = true;
    simple_platformer::World world;
    const simple_platformer::ActorId id = world.addActor(actor);

    simple_platformer::updateActorMovement(map, world, 0.1F);

    simple_platformer::Actor& moved = tests::actor(world, id);
    REQUIRE(moved.body.bounds.topLeft.x == 16.0F);
    REQUIRE(moved.body.velocity.x == 0.0F);
    REQUIRE(moved.body.bounds.topLeft.y > 4.0F);
    REQUIRE(moved.body.velocity.y > 0.0F);
}

TEST_CASE("Aim direction controls horizontal facing independently of movement", "[actor][movement]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"..........", "##########"});
    simple_platformer::Actor actor = makeActor({22.0F, 16.0F});
    tests::platformerMovement(actor).grounded = true;
    actor.intentions.direction.x = 1.0F;
    actor.intentions.aimDirection = {-1.0F, -1.0F};
    simple_platformer::World world;
    const simple_platformer::ActorId id = world.addActor(actor);

    simple_platformer::updateActorMovement(map, world, 0.1F);

    simple_platformer::Actor& moved = tests::actor(world, id);
    REQUIRE(moved.body.velocity.x > 0.0F);
    REQUIRE(moved.facing == simple_platformer::Facing::Left);
}

TEST_CASE("Fast walking accelerates and stops before a ledge", "[actor][movement]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "###..###"});
    simple_platformer::Actor walker = makeActor({24.0F, 32.0F});
    tests::platformerMovement(walker).config.maximumSpeed = 125.0F;
    tests::platformerMovement(walker).grounded = true;
    walker.intentions.direction.x = 1.0F;
    walker.intentions.avoidLedges = true;
    simple_platformer::World world;
    const auto id = world.addActor(walker);

    simple_platformer::updateActorMovement(map, world, 0.05F);
    REQUIRE(tests::actor(world, id).body.velocity.x == 40.0F);
    REQUIRE(tests::actor(world, id).facing == simple_platformer::Facing::Right);
    REQUIRE_FALSE(tests::platformerMovement(tests::actor(world, id)).blocked);
    for (int tick = 0; tick < 20 && !tests::platformerMovement(tests::actor(world, id)).blocked;
         ++tick)
    {
        simple_platformer::updateActorMovement(map, world, 0.05F);
    }
    REQUIRE(tests::platformerMovement(tests::actor(world, id)).blocked);
    REQUIRE(tests::platformerMovement(tests::actor(world, id)).grounded);
    REQUIRE(tests::actor(world, id).body.velocity.x == 0.0F);
    REQUIRE(tests::actor(world, id).body.bounds.topLeft.x + 12.0F <= 48.0F);

    tests::actor(world, id).intentions.direction.x = -1.0F;
    simple_platformer::updateActorMovement(map, world, 0.05F);
    REQUIRE_FALSE(tests::platformerMovement(tests::actor(world, id)).blocked);
    REQUIRE(tests::actor(world, id).body.velocity.x == -40.0F);
}

TEST_CASE("Walking reports a wall independently of combat", "[actor][movement]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "....#...", "########"});
    simple_platformer::Actor walker = makeActor({24.0F, 32.0F});
    tests::platformerMovement(walker).grounded = true;
    walker.intentions.direction.x = 1.0F;
    simple_platformer::World world;
    const auto id = world.addActor(walker);
    for (int tick = 0; tick < 6; ++tick)
    {
        simple_platformer::updateActorMovement(map, world, 0.1F);
    }
    REQUIRE(tests::platformerMovement(tests::actor(world, id)).blocked);
    REQUIRE(tests::actor(world, id).body.velocity.x == 0.0F);
    REQUIRE(tests::actor(world, id).body.bounds.topLeft.x <= 52.0F);

    tests::actor(world, id).intentions = {};
    simple_platformer::updateActorMovement(map, world, 0.1F);
    REQUIRE_FALSE(tests::platformerMovement(tests::actor(world, id)).blocked);
}
