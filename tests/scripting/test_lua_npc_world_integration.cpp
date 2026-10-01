#include <catch2/catch_test_macros.hpp>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/npc/npc_activity.hpp"
#include "simple_platformer/npc/npc_state_machine.hpp"
#include "simple_platformer/npc/npc_system.hpp"
#include "lua_npc_scripts.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_simulation.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/npc_machine_builder.hpp"
#include "support/require_near.hpp"
#include "support/tile_map_builder.hpp"

TEST_CASE("An NPC machine invokes a loaded Lua activity", "[lua][npc][integration]")
{
    simple_platformer::LuaNpcScripts scripts;
    scripts.loadScriptText(
        "fixture",
        "return {activities={flee={update=function() return "
        "{direction={x=-1,y=0},jumpHeld=true} end}}}",
        "fixture.lua");
    const simple_platformer::TileMap map = tests::TileMapBuilder({"...", "...", "###"});
    simple_platformer::World world;
    const simple_platformer::ActorId npc =
        world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                           .atFeet({24.0F, 32.0F})
                           .flying(20.0F)
                           .thinking({})
                           .running(tests::NpcMachineBuilder::named("fixture").state(
                               "fleeing", simple_platformer::LuaNpcActivity{"fixture", "flee"})));

    simple_platformer::updateNpcBehaviour(map, world, 0.1F, &scripts);

    REQUIRE(tests::actor(world, npc).intentions.direction == glm::vec2{-1.0F, 0.0F});
    REQUIRE(tests::actor(world, npc).intentions.jumpHeld);
    REQUIRE(scripts.diagnostics().empty());
}

TEST_CASE(
    "Scripted walking and contact damage stop through a blocked-movement transition",
    "[lua][npc][integration]")
{
    simple_platformer::LuaNpcScripts scripts;
    // A fixed engine-boundary fixture, not the shipped enemy's tunable policy.
    scripts.loadScriptText("walker", R"(
        return {activities = {
            walk = {
                enter = function(self) self.direction = 1 end,
                update = function(self)
                    return {direction = {x = self.direction, y = 0},
                            avoidLedges = true, contactDamage = true}
                end
            },
            rest = {update = function() return {} end}
        }}
    )");
    simple_platformer::TileMap map = tests::TileMapBuilder({"........", "........", "###..###"});
    simple_platformer::World world;
    simple_platformer::PlatformerMovementConfig movement;
    movement.maximumSpeed = 125.0F;
    const simple_platformer::ActorId npc = world.addActor(
        tests::ActorBuilder::sized({12.0F, 12.0F})
            .atFeet({24.0F, 32.0F})
            .platforming(movement)
            .onTeam(simple_platformer::Team::Enemy)
            .withContactDamage()
            .thinking({})
            .running(tests::NpcMachineBuilder::named("walker")
                         .state("moving", simple_platformer::LuaNpcActivity{"walker", "walk"})
                         .state("resting", simple_platformer::LuaNpcActivity{"walker", "rest"})
                         .transition("moving", "resting")
                         .when("movementBlocked", true)));
    tests::platformerMovement(tests::actor(world, npc)).grounded = true;
    constexpr float StepSeconds = 0.05F;

    simple_platformer::updateWorldSimulation(map, world, StepSeconds, nullptr, &scripts);
    REQUIRE(simple_platformer::activeNpcMachineState(tests::machine(world, npc)).name == "moving");
    REQUIRE_NEAR(tests::actor(world, npc).body.velocity.x, 40.0F);
    REQUIRE(tests::contactDamage(world, npc).active);

    constexpr int MaxStepsToRest = 20;
    int stepsToRest = 0;
    while (stepsToRest < MaxStepsToRest &&
           simple_platformer::activeNpcMachineState(tests::machine(world, npc)).name == "moving")
    {
        simple_platformer::updateWorldSimulation(map, world, StepSeconds, nullptr, &scripts);
        ++stepsToRest;
    }
    CAPTURE(stepsToRest);
    REQUIRE(simple_platformer::activeNpcMachineState(tests::machine(world, npc)).name == "resting");
    REQUIRE_FALSE(tests::contactDamage(world, npc).active);
    REQUIRE(tests::actor(world, npc).intentions.direction.x == 0.0F);
    REQUIRE(tests::actor(world, npc).body.velocity.x == 0.0F);
    REQUIRE(tests::platformerMovement(tests::actor(world, npc)).grounded);
    REQUIRE(scripts.diagnostics().empty());
}
