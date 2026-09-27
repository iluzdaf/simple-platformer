#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <utility>

#include "content/machine_catalog.hpp"
#include "content/npc_script_catalog.hpp"
#include "simple_platformer/npc/npc_activity.hpp"
#include "simple_platformer/npc/npc_state_machine.hpp"
#include "simple_platformer/scripting/lua_npc_scripts.hpp"

using Catch::Matchers::ContainsSubstring;

namespace
{
    simple_platformer::MachineCatalog catalogWith(simple_platformer::LuaNpcActivity activity)
    {
        simple_platformer::NpcStateMachine machine;
        machine.name = "test";
        machine.states.push_back({"waiting", std::move(activity)});
        return {{machine.name, std::move(machine)}};
    }
}

TEST_CASE("Machine scripts load from the content scripts directory", "[app][machines][lua]")
{
    simple_platformer::LuaNpcScripts scripts;
    const auto machines = catalogWith({"example_npc", "idle"});

    simple_platformer::loadNpcActivityScripts(scripts, machines, "tests/fixtures/scripts");

    REQUIRE(scripts.hasActivity({"example_npc", "idle"}));
}

TEST_CASE("Machine script loading rejects unresolved and unsafe references", "[app][machines][lua]")
{
    simple_platformer::LuaNpcScripts scripts;

    SECTION("Unknown activity")
    {
        const auto machines = catalogWith({"example_npc", "missing"});
        REQUIRE_THROWS_WITH(
            simple_platformer::loadNpcActivityScripts(scripts, machines, "tests/fixtures/scripts"),
            ContainsSubstring("test") && ContainsSubstring("example_npc.missing"));
    }
    SECTION("A script name cannot escape the scripts directory")
    {
        const auto machines = catalogWith({"../example_npc", "idle"});
        REQUIRE_THROWS_WITH(
            simple_platformer::loadNpcActivityScripts(scripts, machines, "tests/fixtures/scripts"),
            ContainsSubstring("must be a file stem"));
    }
}
