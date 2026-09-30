#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/npc/npc_activity.hpp"
#include "simple_platformer/npc/npc_activity_scripts.hpp"
#include "lua_npc_scripts.hpp"

namespace
{
    constexpr simple_platformer::ActorId FirstActor{1};
    const simple_platformer::LuaNpcActivity Activity{"example", "decide"};
}

TEST_CASE("A failing Lua update reports its context and asks for nothing", "[lua][npc]")
{
    simple_platformer::LuaNpcScripts scripts;
    scripts.loadScriptText(
        "example",
        "return {activities={decide={update=function() error('boom') end}}}",
        "failing.lua");
    const simple_platformer::NpcActivitySnapshot snapshot;
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
    REQUIRE_THAT(diagnostic.message, Catch::Matchers::ContainsSubstring("boom"));
}

TEST_CASE("Lua commands reject unknown fields and non-finite vectors", "[lua][npc]")
{
    simple_platformer::LuaNpcScripts scripts;
    const simple_platformer::NpcActivitySnapshot snapshot;

    SECTION("Unknown field")
    {
        scripts.loadScriptText(
            "example",
            "return {activities={decide={update=function() return {teleport=true} end}}}");
        scripts.enter(FirstActor, Activity, snapshot);

        REQUIRE_FALSE(scripts.update(FirstActor, Activity, snapshot, 0.1F).intentions.jumpPressed);
        REQUIRE_THAT(
            scripts.diagnostics().back().message, Catch::Matchers::ContainsSubstring("teleport"));
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
        REQUIRE_THAT(
            scripts.diagnostics().back().message, Catch::Matchers::ContainsSubstring("finite"));
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
    simple_platformer::LuaNpcScripts scripts;
    scripts.loadScriptText(
        "example", "return {activities={decide={update=function() return {" + field + "=1} end}}}");
    const simple_platformer::NpcActivitySnapshot snapshot;
    scripts.enter(FirstActor, Activity, snapshot);
    const auto command = scripts.update(FirstActor, Activity, snapshot, 0.1F);
    REQUIRE_FALSE(command.intentions.contactDamage);
    REQUIRE_FALSE(command.intentions.avoidLedges);
    REQUIRE_THAT(scripts.diagnostics().back().message, Catch::Matchers::ContainsSubstring(field));
}

TEST_CASE("A Lua climb grip is named, and keeps the grip when left out", "[lua][npc]")
{
    std::string value;
    SECTION("A name it does not know")
    {
        value = "\"grab\"";
    }
    SECTION("A boolean")
    {
        value = "true";
    }
    simple_platformer::LuaNpcScripts scripts;
    scripts.loadScriptText(
        "example",
        "return {activities={decide={update=function() return {climbGrip=" + value + "} end}}}");
    const simple_platformer::NpcActivitySnapshot snapshot;
    scripts.enter(FirstActor, Activity, snapshot);
    const auto command = scripts.update(FirstActor, Activity, snapshot, 0.1F);
    REQUIRE(command.intentions.climbGrip == simple_platformer::ClimbGrip::Keep);
    REQUIRE_THAT(
        scripts.diagnostics().back().message,
        Catch::Matchers::ContainsSubstring(
            "command.climbGrip must be \"keep\", \"hold\" or \"release\""));
}

TEST_CASE("A Lua command that leaves out the climb grip keeps it", "[lua][npc]")
{
    simple_platformer::LuaNpcScripts scripts;
    scripts.loadScriptText(
        "example", "return {activities={decide={update=function() return {} end}}}");
    const simple_platformer::NpcActivitySnapshot snapshot;
    scripts.enter(FirstActor, Activity, snapshot);
    const auto command = scripts.update(FirstActor, Activity, snapshot, 0.1F);
    REQUIRE(command.intentions.climbGrip == simple_platformer::ClimbGrip::Keep);
    REQUIRE(scripts.diagnostics().empty());
}

TEST_CASE("Lua activities cannot use filesystem or system libraries", "[lua][npc]")
{
    simple_platformer::LuaNpcScripts scripts;
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
    const simple_platformer::NpcActivitySnapshot snapshot;
    scripts.enter(FirstActor, Activity, snapshot);

    REQUIRE(scripts.update(FirstActor, Activity, snapshot, 0.1F).intentions.direction.x == 0.0F);
}

TEST_CASE("A Lua activity cannot run past its instruction budget", "[lua][npc]")
{
    simple_platformer::LuaNpcScripts scripts;
    scripts.loadScriptText(
        "example",
        "return {activities={decide={update=function() while true do end end}}}",
        "loop.lua");
    const simple_platformer::NpcActivitySnapshot snapshot;
    scripts.enter(FirstActor, Activity, snapshot);

    REQUIRE_NOTHROW(scripts.update(FirstActor, Activity, snapshot, 0.1F));
    REQUIRE_THAT(
        scripts.diagnostics().back().message,
        Catch::Matchers::ContainsSubstring("instruction budget"));
}

TEST_CASE("A Lua activity rejects an invalid update time step", "[lua][npc]")
{
    simple_platformer::LuaNpcScripts scripts;
    scripts.loadScriptText(
        "example", "return {activities={decide={update=function() return {} end}}}");
    const simple_platformer::NpcActivitySnapshot snapshot;
    scripts.enter(FirstActor, Activity, snapshot);

    REQUIRE_THROWS_WITH(
        scripts.update(FirstActor, Activity, snapshot, -0.1F),
        "NPC script update time step must be a finite, non-negative number of seconds");
}
