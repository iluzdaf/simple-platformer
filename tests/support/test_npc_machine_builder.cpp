#include <catch2/catch_test_macros.hpp>

#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_activity.hpp"
#include "simple_platformer/npc/npc_state_machine.hpp"
#include "support/npc_machine_builder.hpp"

using simple_platformer::NpcState;
using tests::NpcMachineBuilder;

TEST_CASE(
    "The machine builder keeps states and transitions in the order stated",
    "[support][machine-builder]")
{
    const simple_platformer::NpcStateMachine machine = NpcMachineBuilder::named("test")
                                                           .state("rest", NpcState::Idle)
                                                           .state("hunt", NpcState::Chase)
                                                           .transition("rest", "hunt")
                                                           .when("targetKnown", true)
                                                           .transition("hunt", "rest")
                                                           .when("targetKnown", false)
                                                           .after(0.5F);
    REQUIRE(machine.name == "test");
    REQUIRE(machine.states.size() == 2);
    REQUIRE(machine.states[1].name == "hunt");
    REQUIRE(
        std::get<simple_platformer::BuiltInNpcActivity>(machine.states[1].does).state ==
        NpcState::Chase);
    REQUIRE(machine.transitions.size() == 2);
    REQUIRE(machine.transitions[0].after == 0.0F);
    REQUIRE(machine.transitions[1].from == "hunt");
    REQUIRE(machine.transitions[1].when.at("targetKnown") == false);
    REQUIRE(machine.transitions[1].after == 0.5F);
}

TEST_CASE("The machine builder accepts a named Lua activity", "[support][machine-builder][lua]")
{
    const simple_platformer::NpcStateMachine machine =
        NpcMachineBuilder::named("scripted")
            .state("fleeing", simple_platformer::LuaNpcActivity{"rat", "flee"});

    REQUIRE(
        std::get<simple_platformer::LuaNpcActivity>(machine.states.front().does) ==
        simple_platformer::LuaNpcActivity{"rat", "flee"});
}
