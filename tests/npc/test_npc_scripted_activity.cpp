#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include <string>
#include <utility>
#include <vector>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/actor/actor_system.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/navigation/platformer_connection_cache.hpp"
#include "simple_platformer/navigation/navigation_fill.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_activity.hpp"
#include "simple_platformer/npc/npc_activity_script.hpp"
#include "simple_platformer/npc/npc_state_machine.hpp"
#include "simple_platformer/npc/npc_system.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"
#include "simple_platformer/world/world_simulation.hpp"
#include "support/tile_map_builder.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/add_player.hpp"
#include "support/npc_machine_builder.hpp"
#include "support/fixed_step.hpp"
#include "support/tile_size.hpp"

using tests::actor;
using tests::brain;
using tests::machine;

namespace
{
    tests::ActorBuilder makePlayer(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F}).atFeet(feet).walking();
    }

    tests::ActorBuilder::Thinking makeNpc(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F})
            .atFeet(feet)
            .flying(20.0F)
            .thinking({64.0F, 1.0F});
    }

    struct ScriptCall
    {
        std::string hook;
        simple_platformer::ActorId actor;
        simple_platformer::LuaNpcActivity activity;
        simple_platformer::NpcActivitySnapshot snapshot;
    };

    class RecordingNpcScripts final : public simple_platformer::NpcActivityScripts
    {
    public:
        void enter(
            simple_platformer::ActorId actor,
            const simple_platformer::LuaNpcActivity& activity,
            const simple_platformer::NpcActivitySnapshot& snapshot) override
        {
            calls.push_back({"enter", actor, activity, snapshot});
        }

        simple_platformer::NpcActivityCommand update(
            simple_platformer::ActorId actor,
            const simple_platformer::LuaNpcActivity& activity,
            const simple_platformer::NpcActivitySnapshot& snapshot,
            float deltaTime) override
        {
            updateSteps.push_back(deltaTime);
            calls.push_back({"update", actor, activity, snapshot});
            return command;
        }

        void exit(
            simple_platformer::ActorId actor,
            const simple_platformer::LuaNpcActivity& activity,
            const simple_platformer::NpcActivitySnapshot& snapshot) override
        {
            calls.push_back({"exit", actor, activity, snapshot});
        }

        void forget(simple_platformer::ActorId actor) override
        {
            forgotten.push_back(actor);
        }

        simple_platformer::NpcActivityCommand command;
        std::vector<ScriptCall> calls;
        std::vector<float> updateSteps;
        std::vector<simple_platformer::ActorId> forgotten;
    };
}

TEST_CASE("A scripted route follows a climbing path", "[npc][lua][climb]")
{
    simple_platformer::TileMap map = tests::TileMapBuilder({"..............",
                                                            "..cccccccccc..",
                                                            ".c..........c.",
                                                            ".c.########.c.",
                                                            ".c.########.c.",
                                                            ".c.########.c.",
                                                            "..##########..",
                                                            ".............."})
                                         .where('c', tests::Tile{}.blocksMovement().climbable());
    simple_platformer::World world;
    simple_platformer::Actor npc =
        tests::ActorBuilder::sized({12.0F, 12.0F})
            .inCell({2, 5})
            .walking()
            .climbing({60.0F})
            .thinking({})
            .running(tests::NpcMachineBuilder::named("climber").state(
                "route", simple_platformer::LuaNpcActivity{"climber", "route"}));
    tests::platformerMovement(npc).grounded = true;
    const simple_platformer::ActorId npcId = world.addActor(npc);
    RecordingNpcScripts scripts;
    scripts.command.routeTo = simple_platformer::feetInCell(tests::TileSize, {11, 5});

    bool requestedClimb = false;
    bool reachedCeiling = false;
    for (int tick = 0; tick < 1500 && !reachedCeiling; ++tick)
    {
        simple_platformer::updateWorldSimulation(
            map, world, tests::FixedStepSeconds, nullptr, &scripts);
        requestedClimb = requestedClimb || actor(world, npcId).intentions.climbGrip ==
                                               simple_platformer::ClimbGrip::Hold;
        reachedCeiling =
            tests::surfaceClimb(world, npcId).surface == simple_platformer::ClimbSurface::Ceiling;
    }
    REQUIRE(requestedClimb);
    REQUIRE(reachedCeiling);
}

TEST_CASE("A machine reacts to landing and blocked walking facts", "[npc][machine][movement]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", ".....#....", "##########"});
    simple_platformer::World world;
    const auto playerId = tests::addPlayer(world, makePlayer({56.0F, 32.0F}));
    actor(world, playerId).team = simple_platformer::Team::Player;
    auto charger =
        tests::ActorBuilder::sized({12.0F, 12.0F})
            .atFeet({24.0F, 32.0F})
            .walking()
            .onTeam(simple_platformer::Team::Enemy)
            .thinking({80.0F, 1.0F})
            .running(tests::NpcMachineBuilder::named("charger")
                         .state("sleep", simple_platformer::LuaNpcActivity{"test", "rest"})
                         .state("charge", simple_platformer::LuaNpcActivity{"test", "walk"})
                         .state("stunned", simple_platformer::LuaNpcActivity{"test", "rest"})
                         .transition("sleep", "charge")
                         .when("heardLanding", true)
                         .when("targetOnSameRun", true)
                         .when("targetWithinNoticeDistance", true)
                         .transition("charge", "stunned")
                         .when("movementBlocked", true))
            .withContactDamage();
    const auto npcId = world.addActor(std::move(charger));
    tests::platformerMovement(actor(world, npcId)).grounded = true;
    tests::platformerMovement(actor(world, playerId)).grounded = true;
    RecordingNpcScripts scripts;
    scripts.command.intentions.direction.x = 1.0F;
    scripts.command.intentions.avoidLedges = true;
    scripts.command.intentions.contactDamage = true;
    world.emitNoise({playerId, {56.0F, 32.0F}, simple_platformer::NoiseKind::Landing});

    simple_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE(tests::perception(world, npcId).heardLanding);
    REQUIRE(brain(world, npcId).target == playerId);
    REQUIRE(simple_platformer::onSameGroundRun(
        map, actor(world, npcId).body.bounds, actor(world, playerId).body.bounds));
    simple_platformer::updateNpcBehaviour(map, world, 0.1F, &scripts);
    REQUIRE(machine(world, npcId).definition.states[machine(world, npcId).active].name == "charge");
    REQUIRE(actor(world, npcId).intentions.direction.x == 1.0F);
    REQUIRE(actor(world, npcId).intentions.contactDamage);
    REQUIRE(actor(world, npcId).intentions.avoidLedges);

    for (int tick = 0; tick < 8 && !tests::platformerMovement(actor(world, npcId)).blocked; ++tick)
    {
        simple_platformer::updateActorMovement(map, world, 0.1F);
    }
    REQUIRE(tests::platformerMovement(actor(world, npcId)).blocked);
    scripts.command = {};
    simple_platformer::updateNpcSenses(map, world, 0.1F);
    simple_platformer::updateNpcBehaviour(map, world, 0.1F, &scripts);
    REQUIRE(
        machine(world, npcId).definition.states[machine(world, npcId).active].name == "stunned");
    REQUIRE(scripts.calls.back().snapshot.facts.movementBlocked);
    REQUIRE_FALSE(actor(world, npcId).intentions.contactDamage);
    REQUIRE(actor(world, npcId).intentions.direction.x == 0.0F);
}

TEST_CASE(
    "A scripted machine activity receives snapshots and returns engine commands",
    "[npc][lua]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "........", "########"});
    simple_platformer::World world;
    const simple_platformer::ActorId npcId = world.addActor(
        makeNpc({24.0F, 32.0F})
            .running(tests::NpcMachineBuilder::named("scripted")
                         .state("roam", simple_platformer::LuaNpcActivity{"rat", "roam"}))
            .patrolling({24.0F, 32.0F}, {72.0F, 32.0F}));
    RecordingNpcScripts scripts;
    scripts.command.routeTo = glm::vec2{72.0F, 32.0F};
    scripts.command.aimAt = glm::vec2{80.0F, 16.0F};
    scripts.command.intentions.primaryAttackPressed = true;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F, &scripts);

    REQUIRE(scripts.calls.size() == 2);
    REQUIRE(scripts.calls[0].hook == "enter");
    REQUIRE(scripts.calls[1].hook == "update");
    REQUIRE(scripts.calls[0].actor == npcId);
    REQUIRE(scripts.calls[0].activity == simple_platformer::LuaNpcActivity{"rat", "roam"});
    REQUIRE(scripts.calls[0].snapshot.feet == glm::vec2{24.0F, 32.0F});
    REQUIRE(scripts.calls[0].snapshot.facts.stateElapsed == 0.0F);
    REQUIRE(scripts.calls[1].snapshot.facts.stateElapsed == 0.0F);
    REQUIRE_FALSE(scripts.calls[0].snapshot.routeComplete);
    REQUIRE_FALSE(scripts.calls[0].snapshot.targetFeet.has_value());
    REQUIRE(scripts.calls[0].snapshot.patrol.has_value());
    const simple_platformer::Patrol scriptedPatrol =
        scripts.calls[0].snapshot.patrol.value_or(simple_platformer::Patrol{});
    REQUIRE(scriptedPatrol.firstFeet == glm::vec2{24.0F, 32.0F});
    REQUIRE(scriptedPatrol.secondFeet == glm::vec2{72.0F, 32.0F});
    REQUIRE(scripts.updateSteps == std::vector<float>{0.1F});
    REQUIRE(actor(world, npcId).intentions.direction.x > 0.0F);
    REQUIRE(actor(world, npcId).intentions.aimDirection == glm::vec2{56.0F, -16.0F});
    REQUIRE(actor(world, npcId).intentions.primaryAttackPressed);
    REQUIRE(machine(world, npcId).stateElapsed == 0.1F);
    REQUIRE(brain(world, npcId).stateElapsed == 0.0F);

    simple_platformer::updateNpcBehaviour(map, world, 0.1F, &scripts);
    REQUIRE(scripts.calls.size() == 3);
    REQUIRE(scripts.calls.back().hook == "update");
    REQUIRE(scripts.calls.back().snapshot.facts.stateElapsed == 0.1F);
}

TEST_CASE("A scripted machine exits and enters around a transition", "[npc][lua]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = tests::addPlayer(world, makePlayer({56.0F, 32.0F}));
    const simple_platformer::ActorId npcId = world.addActor(
        makeNpc({24.0F, 32.0F})
            .running(tests::NpcMachineBuilder::named("scripted")
                         .state("waiting", simple_platformer::LuaNpcActivity{"rat", "wait"})
                         .state("moving", simple_platformer::LuaNpcActivity{"rat", "move"})
                         .transition("waiting", "moving")
                         .when("targetKnown", true)));
    RecordingNpcScripts scripts;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F, &scripts);
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastKnownTargetFeet = {56.0F, 32.0F};
    tests::perception(world, npcId).targetVisible = true;
    simple_platformer::updateNpcBehaviour(map, world, 0.1F, &scripts);

    REQUIRE(scripts.calls.size() == 5);
    REQUIRE(scripts.calls[0].hook == "enter");
    REQUIRE(scripts.calls[0].activity.activity == "wait");
    REQUIRE(scripts.calls[1].hook == "update");
    REQUIRE(scripts.calls[2].hook == "exit");
    REQUIRE(scripts.calls[2].activity.activity == "wait");
    REQUIRE(scripts.calls[2].snapshot.facts.stateElapsed == 0.1F);
    REQUIRE(scripts.calls[3].hook == "enter");
    REQUIRE(scripts.calls[3].activity.activity == "move");
    REQUIRE(scripts.calls[3].snapshot.facts.stateElapsed == 0.0F);
    REQUIRE(scripts.calls[3].snapshot.targetFeet == glm::vec2{56.0F, 32.0F});
    REQUIRE(scripts.calls[4].hook == "update");
    REQUIRE(scripts.calls[4].snapshot.facts.stateElapsed == 0.0F);
    REQUIRE(simple_platformer::activeNpcMachineState(machine(world, npcId)).name == "moving");
}

TEST_CASE("A scripted machine activity requires a scripting runtime", "[npc][lua][validation]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"...", "...", "###"});
    simple_platformer::World world;
    world.addActor(
        makeNpc({24.0F, 32.0F})
            .running(tests::NpcMachineBuilder::named("scripted")
                         .state("waiting", simple_platformer::LuaNpcActivity{"rat", "wait"})));

    REQUIRE_THROWS_WITH(
        simple_platformer::updateNpcBehaviour(map, world, 0.1F),
        "A scripted NPC activity needs the scripting runtime");
}

TEST_CASE("Removing an actor forgets its scripted activity state", "[npc][lua][lifecycle]")
{
    simple_platformer::World world;
    const simple_platformer::ActorId npcId = world.addActor(makeNpc({24.0F, 32.0F}));
    simple_platformer::WorldRequests requests;
    requests.remove(npcId);
    RecordingNpcScripts scripts;

    simple_platformer::forgetNpcActivities(requests.actorsToRemove(), scripts);
    REQUIRE(world.findActor(npcId) != nullptr);
    simple_platformer::applyWorldRequests(world, requests);

    REQUIRE(scripts.forgotten == std::vector<simple_platformer::ActorId>{npcId});
    REQUIRE(world.findActor(npcId) == nullptr);
}
