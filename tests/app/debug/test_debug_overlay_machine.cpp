#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "debug/debug_overlay.hpp"
#include "debug/navigation_debug.hpp"
#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_state_machine.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/actor_builder.hpp"
#include "support/npc_facts_builder.hpp"
#include "support/npc_machine_builder.hpp"
#include "support/actor_components.hpp"
#include "support/tile_map_builder.hpp"
#include "support/add_player.hpp"
#include "support/fixed_step.hpp"

TEST_CASE("The overlay shows a machine state in place of the built-in state", "[app][debug]")
{
    simple_platformer::World world;
    world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                       .at({16.0F, 32.0F})
                       .walking()
                       .thinking({})
                       .running(tests::NpcMachineBuilder::named("test").state(
                           "rest", simple_platformer::NpcState::Idle)));
    const simple_platformer::TileMap map = tests::TileMapBuilder({"......", "######"});
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};

    const simple_platformer::DebugOverlay debug = simple_platformer::makeDebugOverlay(
        world, map, cameraController, 128.0F, tests::FixedStepSeconds);

    REQUIRE(debug.actors.size() == 1);
    REQUIRE(debug.actors.front().machineState == "rest");
    REQUIRE_FALSE(debug.actors.front().npcState.has_value());
    REQUIRE_FALSE(debug.actors.front().npcTactic.has_value());
}

namespace
{
    // An NPC on the ground running a two-state machine, for the machine window's tests.
    simple_platformer::Actor machineNpc(glm::vec2 topLeft)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F})
            .at(topLeft)
            .walking()
            .thinking({})
            .running(tests::NpcMachineBuilder::named("test")
                         .state("rest", simple_platformer::NpcState::Idle)
                         .state("hunt", simple_platformer::NpcState::Chase)
                         .transition("rest", "hunt")
                         .when("targetKnown", true));
    }

    // Which NPC the machine window follows, or nothing.
    std::optional<simple_platformer::ActorId> followedBy(
        const simple_platformer::DebugOverlay& debug)
    {
        if (!debug.machine.has_value())
        {
            return std::nullopt;
        }
        return debug.machine.value_or(simple_platformer::MachineDebugInfo{}).actor;
    }

    simple_platformer::DebugOverlay overlayOf(
        const simple_platformer::World& world,
        std::optional<glm::vec2> cursorWorld = std::nullopt,
        std::optional<simple_platformer::ActorId> lockedMachineActor = std::nullopt)
    {
        const simple_platformer::TileMap map = tests::TileMapBuilder({"......", "######"});
        const simple_platformer::CameraController cameraController{
            simple_platformer::Camera{}, {80.0F, 40.0F}};
        simple_platformer::NavigationDebugView view;
        view.cursorWorld = cursorWorld;
        return simple_platformer::makeDebugOverlay(
            world,
            map,
            cameraController,
            128.0F,
            tests::FixedStepSeconds,
            view,
            lockedMachineActor);
    }
}

TEST_CASE("The machine window follows the NPC with a machine nearest the player", "[app][debug]")
{
    simple_platformer::World world;
    tests::addPlayer(
        world, tests::ActorBuilder::sized({12.0F, 12.0F}).at({100.0F, 20.0F}).walking());
    // The nearest NPC has no machine, so the nearer of the two that do is followed.
    world.addActor(
        tests::ActorBuilder::sized({12.0F, 12.0F}).at({110.0F, 20.0F}).walking().thinking({}));
    world.addActor(machineNpc({200.0F, 20.0F}));
    const simple_platformer::ActorId nearer = world.addActor(machineNpc({60.0F, 20.0F}));

    const simple_platformer::DebugOverlay debug = overlayOf(world);

    REQUIRE(debug.machine.has_value());
    const simple_platformer::MachineDebugInfo machine =
        debug.machine.value_or(simple_platformer::MachineDebugInfo{});
    REQUIRE(machine.actor == nearer);
    REQUIRE(machine.definition.name == "test");
    REQUIRE(machine.definition.states.size() == 2);
    REQUIRE(machine.definition.transitions.size() == 1);
    REQUIRE(machine.active == 0);
    REQUIRE_FALSE(machine.lastFired.has_value());
}

TEST_CASE("The machine window follows the NPC under the cursor instead", "[app][debug]")
{
    simple_platformer::World world;
    tests::addPlayer(
        world, tests::ActorBuilder::sized({12.0F, 12.0F}).at({100.0F, 20.0F}).walking());
    world.addActor(machineNpc({60.0F, 20.0F}));
    const simple_platformer::ActorId further = world.addActor(machineNpc({200.0F, 20.0F}));

    const simple_platformer::DebugOverlay underCursor = overlayOf(world, glm::vec2{206.0F, 26.0F});
    REQUIRE(followedBy(underCursor) == further);
    // The cursor over an NPC without a machine, or over nothing, changes nothing.
    world.addActor(
        tests::ActorBuilder::sized({12.0F, 12.0F}).at({150.0F, 20.0F}).walking().thinking({}));
    REQUIRE(followedBy(overlayOf(world, glm::vec2{156.0F, 26.0F})) != further);
    REQUIRE(followedBy(overlayOf(world, glm::vec2{10.0F, 10.0F})) != further);
}

TEST_CASE("A locked machine actor overrides the cursor", "[app][debug]")
{
    simple_platformer::World world;
    tests::addPlayer(
        world, tests::ActorBuilder::sized({12.0F, 12.0F}).at({100.0F, 20.0F}).walking());
    const simple_platformer::ActorId locked = world.addActor(machineNpc({60.0F, 20.0F}));
    world.addActor(machineNpc({200.0F, 20.0F}));

    const simple_platformer::DebugOverlay debug =
        overlayOf(world, glm::vec2{206.0F, 26.0F}, locked);

    REQUIRE(followedBy(debug) == locked);
}

TEST_CASE("The machine window follows nothing off screen", "[app][debug]")
{
    simple_platformer::World world;
    tests::addPlayer(
        world, tests::ActorBuilder::sized({12.0F, 12.0F}).at({100.0F, 20.0F}).walking());
    const simple_platformer::ActorId offScreen =
        world.addActor(machineNpc({simple_platformer::InternalViewportSize.x + 100.0F, 20.0F}));

    REQUIRE_FALSE(overlayOf(world).machine.has_value());
    REQUIRE(followedBy(overlayOf(world, std::nullopt, offScreen)) == offScreen);
}

TEST_CASE("The machine window is told which transition fired last", "[app][debug]")
{
    simple_platformer::World world;
    const simple_platformer::ActorId id = world.addActor(machineNpc({60.0F, 20.0F}));
    REQUIRE(
        simple_platformer::advanceNpcMachine(
            tests::machine(world, id),
            tests::NpcFactsBuilder::facts().knowingTarget(),
            tests::FixedStepSeconds) == 0);

    const simple_platformer::DebugOverlay debug = overlayOf(world);

    REQUIRE(debug.machine.has_value());
    const simple_platformer::MachineDebugInfo machine =
        debug.machine.value_or(simple_platformer::MachineDebugInfo{});
    REQUIRE(machine.active == 1);
    REQUIRE(machine.lastFired == 0);
}
