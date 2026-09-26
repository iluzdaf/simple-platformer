#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/npc/npc_activity.hpp"
#include "simple_platformer/npc/npc_activity_script.hpp"
#include "simple_platformer/npc/npc_system.hpp"
#include "simple_platformer/scripting/lua_npc_scripts.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
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
        snapshot.facts.targetKnown = true;
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
                                primaryAttackPressed = snapshot.facts.targetKnown,
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
    REQUIRE(command.aimAt == snapshot.targetFeet);
    REQUIRE(command.clearRoute);
    REQUIRE(snapshot.feet.x == 12.0F);
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
