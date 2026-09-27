#include <initializer_list>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "content/npc_script_catalog.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_activity.hpp"
#include "simple_platformer/npc/npc_activity_script.hpp"
#include "simple_platformer/npc/npc_state_machine.hpp"
#include "simple_platformer/npc/npc_system.hpp"
#include "simple_platformer/scripting/lua_npc_scripts.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_simulation.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/require_near.hpp"
#include "support/npc_machine_builder.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    using Catch::Matchers::ContainsSubstring;
    using simple_platformer::ActorId;
    using simple_platformer::LuaNpcActivity;
    using simple_platformer::LuaNpcScripts;
    using simple_platformer::NpcActivitySnapshot;

    constexpr ActorId FirstActor{1};
    constexpr ActorId SecondActor{2};
    const LuaNpcActivity Activity{"example", "decide"};

    NpcActivitySnapshot aSnapshot()
    {
        NpcActivitySnapshot snapshot;
        snapshot.feet = {12.0F, 34.0F};
        snapshot.targetFeet = {{56.0F, 78.0F}};
        snapshot.patrol = simple_platformer::Patrol{{8.0F, 34.0F}, {80.0F, 34.0F}, true};
        snapshot.facts.targetKnown = true;
        snapshot.facts.heardLanding = true;
        snapshot.facts.targetOnSameRun = true;
        snapshot.facts.targetWithinNoticeDistance = true;
        snapshot.facts.targetWithinStandoffDistance = true;
        snapshot.facts.movementBlocked = true;
        snapshot.facts.stateElapsed = 0.25F;
        snapshot.pathComplete = true;
        snapshot.tuning["speed"] = 3.0F;
        return snapshot;
    }
}

TEST_CASE("A Lua activity reads a copied snapshot and returns a command", "[lua][npc]")
{
    LuaNpcScripts scripts;
    scripts.loadScriptText(
        "example",
        R"(
            return {
                activities = {
                    decide = {
                        update = function(self, snapshot, dt)
                            snapshot.feet.x = 999
                            return {
                                direction = {x = snapshot.tuning.speed * dt, y = 0},
                                aimAt = snapshot.targetFeet,
                                routeTo = snapshot.patrol.secondFeet,
                                primaryAttackPressed = snapshot.facts.targetKnown,
                                jumpHeld = snapshot.facts.heardLanding and snapshot.facts.targetOnSameRun,
                                jumpPressed = snapshot.facts.movementBlocked,
                                avoidLedges = snapshot.facts.targetWithinStandoffDistance,
                                contactDamage = snapshot.facts.targetWithinNoticeDistance,
                                clearRoute = snapshot.pathComplete
                            }
                        end
                    }
                }
            }
        )",
        "command.lua");
    NpcActivitySnapshot snapshot = aSnapshot();
    scripts.enter(FirstActor, Activity, snapshot);

    const simple_platformer::NpcActivityCommand command =
        scripts.update(FirstActor, Activity, snapshot, 0.5F);

    REQUIRE(command.intentions.direction.x == 1.5F);
    REQUIRE(command.intentions.direction.y == 0.0F);
    REQUIRE(command.intentions.primaryAttackPressed);
    REQUIRE(command.intentions.jumpHeld);
    REQUIRE(command.intentions.jumpPressed);
    REQUIRE(command.intentions.avoidLedges);
    REQUIRE(command.intentions.contactDamage);
    REQUIRE(command.aimAt == snapshot.targetFeet);
    REQUIRE(command.routeTo == glm::vec2{80.0F, 34.0F});
    REQUIRE(command.clearRoute);
    REQUIRE(snapshot.feet.x == 12.0F);
    REQUIRE(scripts.diagnostics().empty());
}

TEST_CASE("Lua receives independent run and range facts", "[lua][npc]")
{
    LuaNpcScripts scripts;
    scripts.loadScriptText("example", R"(
        return {activities={decide={update=function(self, snapshot)
            return {jumpHeld=snapshot.facts.targetOnSameRun,
                    contactDamage=snapshot.facts.targetWithinNoticeDistance}
        end}}}
    )");
    auto snapshot = aSnapshot();
    scripts.enter(FirstActor, Activity, snapshot);
    for (const bool sameRun : {false, true})
    {
        for (const bool withinRange : {false, true})
        {
            snapshot.facts.targetOnSameRun = sameRun;
            snapshot.facts.targetWithinNoticeDistance = withinRange;
            const auto command = scripts.update(FirstActor, Activity, snapshot, 0.1F);
            REQUIRE(command.intentions.jumpHeld == sameRun);
            REQUIRE(command.intentions.contactDamage == withinRange);
        }
    }
    REQUIRE(scripts.diagnostics().empty());
}

TEST_CASE("An NPC machine invokes a loaded Lua activity", "[lua][npc][integration]")
{
    LuaNpcScripts scripts;
    scripts.loadScriptText(
        "rat",
        "return {activities={flee={update=function() return "
        "{direction={x=-1,y=0},jumpHeld=true} end}}}",
        "rat.lua");
    const simple_platformer::TileMap map = tests::TileMapBuilder({"...", "...", "###"});
    simple_platformer::World world;
    const ActorId npc = world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                                           .atFeet({24.0F, 32.0F})
                                           .flying(20.0F)
                                           .thinking({})
                                           .running(tests::NpcMachineBuilder::named("rat").state(
                                               "fleeing", LuaNpcActivity{"rat", "flee"})));

    simple_platformer::updateNpcBehaviour(map, world, 0.1F, &scripts);

    REQUIRE(tests::actor(world, npc).intentions.direction == glm::vec2{-1.0F, 0.0F});
    REQUIRE(tests::actor(world, npc).intentions.jumpHeld);
    REQUIRE(scripts.diagnostics().empty());
}

TEST_CASE(
    "Scripted walking and contact damage stop through a blocked-movement transition",
    "[lua][npc][integration]")
{
    LuaNpcScripts scripts;
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
    const ActorId npc =
        world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                           .atFeet({24.0F, 32.0F})
                           .walking(movement)
                           .onTeam(simple_platformer::Team::Enemy)
                           .withContactDamage()
                           .thinking({})
                           .running(tests::NpcMachineBuilder::named("walker")
                                        .state("moving", LuaNpcActivity{"walker", "walk"})
                                        .state("resting", LuaNpcActivity{"walker", "rest"})
                                        .transition("moving", "resting")
                                        .when("movementBlocked", true)));
    tests::platformerMovement(tests::actor(world, npc)).grounded = true;

    simple_platformer::updateWorldSimulation(map, world, 0.05F, nullptr, &scripts);
    REQUIRE_NEAR(tests::actor(world, npc).body.velocity.x, 40.0F);
    REQUIRE(tests::contactDamage(world, npc).active);
    for (int tick = 0;
         tick < 20 &&
         simple_platformer::activeNpcMachineState(tests::machine(world, npc)).name == "moving";
         ++tick)
    {
        simple_platformer::updateWorldSimulation(map, world, 0.05F, nullptr, &scripts);
    }
    REQUIRE(simple_platformer::activeNpcMachineState(tests::machine(world, npc)).name == "resting");
    REQUIRE_FALSE(tests::contactDamage(world, npc).active);
    REQUIRE(tests::actor(world, npc).intentions.direction.x == 0.0F);
    REQUIRE(tests::actor(world, npc).body.velocity.x == 0.0F);
    REQUIRE(tests::platformerMovement(tests::actor(world, npc)).grounded);
    REQUIRE(scripts.diagnostics().empty());
}

TEST_CASE("A Lua script can be loaded from an asset file", "[lua][npc]")
{
    LuaNpcScripts scripts;
    scripts.loadScript("fixture", "tests/fixtures/scripts/example_npc.lua");
    const LuaNpcActivity activity{"fixture", "idle"};
    NpcActivitySnapshot snapshot = aSnapshot();
    snapshot.tuning["direction"] = 4.0F;
    scripts.enter(FirstActor, activity, snapshot);

    const simple_platformer::NpcActivityCommand command =
        scripts.update(FirstActor, activity, snapshot, 0.25F);

    REQUIRE(command.intentions.direction.x == 1.0F);
    REQUIRE(command.intentions.primaryAttackPressed);
}

TEST_CASE("Each actor and each visit has its own Lua activity memory", "[lua][npc]")
{
    LuaNpcScripts scripts;
    scripts.loadScriptText(
        "example",
        R"(
            return {
                activities = {
                    decide = {
                        enter = function(self) self.updates = 10 end,
                        update = function(self)
                            self.updates = self.updates + 1
                            return {direction = {x = self.updates, y = 0}}
                        end
                    }
                }
            }
        )");
    const NpcActivitySnapshot snapshot = aSnapshot();
    scripts.enter(FirstActor, Activity, snapshot);
    scripts.enter(SecondActor, Activity, snapshot);

    REQUIRE(scripts.update(FirstActor, Activity, snapshot, 0.1F).intentions.direction.x == 11.0F);
    REQUIRE(scripts.update(FirstActor, Activity, snapshot, 0.1F).intentions.direction.x == 12.0F);
    REQUIRE(scripts.update(SecondActor, Activity, snapshot, 0.1F).intentions.direction.x == 11.0F);

    scripts.exit(FirstActor, Activity, snapshot);
    scripts.enter(FirstActor, Activity, snapshot);
    REQUIRE(scripts.update(FirstActor, Activity, snapshot, 0.1F).intentions.direction.x == 11.0F);

    scripts.forget(SecondActor);
    REQUIRE(scripts.update(SecondActor, Activity, snapshot, 0.1F).intentions.direction.x == 0.0F);
    REQUIRE(scripts.diagnostics().back().message == "activity was not entered");
}

TEST_CASE("A broken reload leaves the working Lua script in place", "[lua][npc]")
{
    LuaNpcScripts scripts;
    scripts.loadScriptText(
        "example",
        "return {activities={decide={update=function() return {jumpPressed=true} end}}}",
        "working.lua");

    REQUIRE_THROWS_WITH(
        scripts.loadScriptText("example", "return {", "broken.lua"),
        ContainsSubstring("broken.lua"));
    REQUIRE(scripts.hasActivity(Activity));

    const NpcActivitySnapshot snapshot = aSnapshot();
    scripts.enter(FirstActor, Activity, snapshot);
    REQUIRE(scripts.update(FirstActor, Activity, snapshot, 0.1F).intentions.jumpPressed);
}

TEST_CASE("Lua scripts reject activities without an update function", "[lua][npc]")
{
    LuaNpcScripts scripts;

    REQUIRE_THROWS_WITH(
        scripts.loadScriptText(
            "example", "return {activities={wait={enter=function() end}}}", "missing.lua"),
        ContainsSubstring("example.wait"));
}

TEST_CASE("A failing Lua update reports its context and asks for nothing", "[lua][npc]")
{
    LuaNpcScripts scripts;
    scripts.loadScriptText(
        "example",
        "return {activities={decide={update=function() error('boom') end}}}",
        "failing.lua");
    const NpcActivitySnapshot snapshot = aSnapshot();
    scripts.enter(FirstActor, Activity, snapshot);

    const simple_platformer::NpcActivityCommand command =
        scripts.update(FirstActor, Activity, snapshot, 0.1F);

    REQUIRE(command.intentions.direction.x == 0.0F);
    REQUIRE(scripts.diagnostics().size() == 1);
    const simple_platformer::LuaScriptDiagnostic& diagnostic = scripts.diagnostics().front();
    REQUIRE(diagnostic.source == "failing.lua");
    REQUIRE(diagnostic.script == "example");
    REQUIRE(diagnostic.activity == "decide");
    REQUIRE(diagnostic.hook == "update");
    REQUIRE(diagnostic.actor == FirstActor);
    REQUIRE_THAT(diagnostic.message, ContainsSubstring("boom"));
}

TEST_CASE("Lua commands reject unknown fields and non-finite vectors", "[lua][npc]")
{
    LuaNpcScripts scripts;
    const NpcActivitySnapshot snapshot = aSnapshot();

    SECTION("Unknown field")
    {
        scripts.loadScriptText(
            "example",
            "return {activities={decide={update=function() return {teleport=true} end}}}");
        scripts.enter(FirstActor, Activity, snapshot);

        REQUIRE_FALSE(scripts.update(FirstActor, Activity, snapshot, 0.1F).intentions.jumpPressed);
        REQUIRE_THAT(scripts.diagnostics().back().message, ContainsSubstring("teleport"));
    }

    SECTION("Non-finite vector")
    {
        scripts.loadScriptText(
            "example",
            "return {activities={decide={update=function() return "
            "{direction={x=0/0,y=0}} end}}}");
        scripts.enter(FirstActor, Activity, snapshot);

        REQUIRE(
            scripts.update(FirstActor, Activity, snapshot, 0.1F).intentions.direction.x == 0.0F);
        REQUIRE_THAT(scripts.diagnostics().back().message, ContainsSubstring("finite"));
    }
}

TEST_CASE("Lua movement and contact requests require booleans", "[lua][npc]")
{
    std::string field;
    SECTION("Ledge avoidance")
    {
        field = "avoidLedges";
    }
    SECTION("Contact damage")
    {
        field = "contactDamage";
    }
    LuaNpcScripts scripts;
    scripts.loadScriptText(
        "example", "return {activities={decide={update=function() return {" + field + "=1} end}}}");
    const NpcActivitySnapshot snapshot = aSnapshot();
    scripts.enter(FirstActor, Activity, snapshot);
    const auto command = scripts.update(FirstActor, Activity, snapshot, 0.1F);
    REQUIRE_FALSE(command.intentions.contactDamage);
    REQUIRE_FALSE(command.intentions.avoidLedges);
    REQUIRE_THAT(scripts.diagnostics().back().message, ContainsSubstring(field));
}

TEST_CASE("Lua activities cannot use filesystem or system libraries", "[lua][npc]")
{
    LuaNpcScripts scripts;
    scripts.loadScriptText(
        "example",
        R"(
            return {
                activities = {
                    decide = {
                        update = function()
                            local exposed = io or os or package or debug or dofile or loadfile or load
                            return {direction = {x = exposed and 1 or 0, y = 0}}
                        end
                    }
                }
            }
        )");
    const NpcActivitySnapshot snapshot = aSnapshot();
    scripts.enter(FirstActor, Activity, snapshot);

    REQUIRE(scripts.update(FirstActor, Activity, snapshot, 0.1F).intentions.direction.x == 0.0F);
}

TEST_CASE("A Lua activity cannot run past its instruction budget", "[lua][npc]")
{
    LuaNpcScripts scripts;
    scripts.loadScriptText(
        "example",
        "return {activities={decide={update=function() while true do end end}}}",
        "loop.lua");
    const NpcActivitySnapshot snapshot = aSnapshot();
    scripts.enter(FirstActor, Activity, snapshot);

    REQUIRE_NOTHROW(scripts.update(FirstActor, Activity, snapshot, 0.1F));
    REQUIRE_THAT(scripts.diagnostics().back().message, ContainsSubstring("instruction budget"));
}

TEST_CASE("A Lua activity rejects an invalid update time step", "[lua][npc]")
{
    LuaNpcScripts scripts;
    scripts.loadScriptText(
        "example", "return {activities={decide={update=function() return {} end}}}");
    const NpcActivitySnapshot snapshot = aSnapshot();
    scripts.enter(FirstActor, Activity, snapshot);

    REQUIRE_THROWS_WITH(
        scripts.update(FirstActor, Activity, snapshot, -0.1F),
        "NPC script update time step must be a finite, non-negative number of seconds");
}
