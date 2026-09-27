#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/npc/npc_activity.hpp"
#include "simple_platformer/npc/npc_activity_script.hpp"
#include "simple_platformer/scripting/lua_npc_scripts.hpp"

namespace
{
    constexpr simple_platformer::ActorId FirstActor{1};
    const simple_platformer::LuaNpcActivity Activity{"example", "decide"};
}

TEST_CASE("A Lua script can be loaded from an asset file", "[lua][npc]")
{
    simple_platformer::LuaNpcScripts scripts;
    scripts.loadScript("fixture", "tests/fixtures/scripts/example_npc.lua");
    const simple_platformer::LuaNpcActivity activity{"fixture", "idle"};
    simple_platformer::NpcActivitySnapshot snapshot;
    snapshot.facts.targetKnown = true;
    snapshot.tuning["direction"] = 4.0F;
    scripts.enter(FirstActor, activity, snapshot);

    const simple_platformer::NpcActivityCommand command =
        scripts.update(FirstActor, activity, snapshot, 0.25F);

    REQUIRE(command.intentions.direction.x == 1.0F);
    REQUIRE(command.intentions.primaryAttackPressed);
}

TEST_CASE("A broken reload leaves the working Lua script in place", "[lua][npc]")
{
    simple_platformer::LuaNpcScripts scripts;
    scripts.loadScriptText(
        "example",
        "return {activities={decide={update=function() return {jumpPressed=true} end}}}",
        "working.lua");

    REQUIRE_THROWS_WITH(
        scripts.loadScriptText("example", "return {", "broken.lua"),
        Catch::Matchers::ContainsSubstring("broken.lua"));
    REQUIRE(scripts.hasActivity(Activity));

    const simple_platformer::NpcActivitySnapshot snapshot;
    scripts.enter(FirstActor, Activity, snapshot);
    REQUIRE(scripts.update(FirstActor, Activity, snapshot, 0.1F).intentions.jumpPressed);
}

TEST_CASE("Lua scripts reject activities without an update function", "[lua][npc]")
{
    simple_platformer::LuaNpcScripts scripts;

    REQUIRE_THROWS_WITH(
        scripts.loadScriptText(
            "example", "return {activities={wait={enter=function() end}}}", "missing.lua"),
        Catch::Matchers::ContainsSubstring("example.wait"));
}
