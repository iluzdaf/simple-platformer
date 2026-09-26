#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <optional>
#include <stdexcept>

#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_activity.hpp"
#include "simple_platformer/npc/npc_fact_rows.hpp"
#include "simple_platformer/npc/npc_state_machine.hpp"
#include "simple_platformer/npc/npc_transitions.hpp"
#include "support/npc_facts_builder.hpp"
#include "support/npc_machine_builder.hpp"

using Catch::Matchers::ContainsSubstring;
using simple_platformer::NpcFactRow;
using simple_platformer::NpcMachine;
using simple_platformer::NpcState;
using simple_platformer::NpcStateMachine;
using tests::NpcFactsBuilder;
using tests::NpcMachineBuilder;

namespace
{
    // Rest until a target is known, hunt until it is lost for half a second.
    NpcStateMachine restAndHunt()
    {
        return NpcMachineBuilder::named("test")
            .state("rest", NpcState::Idle)
            .state("hunt", NpcState::Chase)
            .transition("rest", "hunt")
            .when("targetKnown", true)
            .transition("hunt", "rest")
            .when("targetKnown", false)
            .after(0.5F);
    }
}

TEST_CASE("Every fact row answers from the facts and an unknown name has no row", "[npc][fsm]")
{
    for (const NpcFactRow& row : simple_platformer::npcFactRows())
    {
        REQUIRE_FALSE(row.holds(NpcFactsBuilder::facts()));
    }
    REQUIRE(simple_platformer::npcFactRow("targetTooClose")
                ->holds(NpcFactsBuilder::facts().targetTooClose()));
    REQUIRE(
        simple_platformer::npcFactRow("hasPatrol")->holds(NpcFactsBuilder::facts().withPatrol()));
    REQUIRE(simple_platformer::npcFactRow("cornered") == nullptr);
}

TEST_CASE("Conditions hold when every fact answers as asked", "[npc][fsm]")
{
    const simple_platformer::NpcFacts facts = NpcFactsBuilder::facts().targetInSights();
    REQUIRE(simple_platformer::npcConditionsHold(
        {{"targetKnown", true}, {"targetInSights", true}}, facts));
    REQUIRE_FALSE(
        simple_platformer::npcConditionsHold({{"targetKnown", true}, {"hasPatrol", true}}, facts));
    REQUIRE(simple_platformer::npcConditionsHold({}, facts));
}

TEST_CASE("A state machine rejects states and transitions it cannot run", "[npc][fsm][validation]")
{
    NpcStateMachine machine = restAndHunt();
    const char* expected = "";
    SECTION("No states")
    {
        machine.states.clear();
        expected = "at least one state";
    }
    SECTION("A state twice")
    {
        machine.states.push_back({"rest", simple_platformer::BuiltInNpcActivity{NpcState::Idle}});
        expected = "declared twice";
    }
    SECTION("A transition from a state it lacks")
    {
        machine.transitions[0].from = "sleep";
        expected = "from \"sleep\" to \"hunt\" starts from a state the machine lacks";
    }
    SECTION("A Lua state without a script name")
    {
        machine.states[0].does = simple_platformer::LuaNpcActivity{"", "wait"};
        expected = "needs a Lua script name";
    }
    SECTION("A Lua state without an activity name")
    {
        machine.states[0].does = simple_platformer::LuaNpcActivity{"rat", ""};
        expected = "needs a Lua activity name";
    }
    SECTION("A transition to a state it lacks")
    {
        machine.transitions[0].to = "pounce";
        expected = "leads to a state the machine lacks";
    }
    SECTION("A condition on a fact no row answers")
    {
        machine.transitions[1].when["cornered"] = true;
        expected = "asks about \"cornered\", and there is no such fact";
    }
    SECTION("A negative hold")
    {
        machine.transitions[1].after = -0.1F;
        expected = "finite, non-negative time";
    }
    REQUIRE_THROWS_WITH(
        simple_platformer::validateNpcStateMachine(machine), ContainsSubstring(expected));
    REQUIRE_THROWS_AS(simple_platformer::startNpcMachine(machine), std::invalid_argument);
}

TEST_CASE(
    "A started machine is in its first state and fires the first transition that holds",
    "[npc][fsm]")
{
    NpcMachine machine = simple_platformer::startNpcMachine(restAndHunt());
    REQUIRE(simple_platformer::activeNpcMachineState(machine).name == "rest");
    REQUIRE(
        std::get<simple_platformer::BuiltInNpcActivity>(
            simple_platformer::activeNpcMachineState(machine).does)
            .state == NpcState::Idle);

    REQUIRE(
        simple_platformer::advanceNpcMachine(machine, NpcFactsBuilder::facts(), 0.1F) ==
        std::nullopt);
    REQUIRE(
        simple_platformer::advanceNpcMachine(
            machine, NpcFactsBuilder::facts().knowingTarget(), 0.1F) == 0);
    REQUIRE(simple_platformer::activeNpcMachineState(machine).name == "hunt");
    REQUIRE(machine.lastFired == 0);
}

TEST_CASE("A transition with a hold fires once its conditions have held that long", "[npc][fsm]")
{
    NpcMachine machine = simple_platformer::startNpcMachine(restAndHunt());
    simple_platformer::advanceNpcMachine(machine, NpcFactsBuilder::facts().knowingTarget(), 0.1F);

    // Lost for 0.3 s, seen again, then lost: the hold starts over.
    REQUIRE(
        simple_platformer::advanceNpcMachine(machine, NpcFactsBuilder::facts(), 0.3F) ==
        std::nullopt);
    REQUIRE(
        simple_platformer::advanceNpcMachine(
            machine, NpcFactsBuilder::facts().knowingTarget(), 0.1F) == std::nullopt);
    REQUIRE(
        simple_platformer::advanceNpcMachine(machine, NpcFactsBuilder::facts(), 0.3F) ==
        std::nullopt);
    REQUIRE(simple_platformer::advanceNpcMachine(machine, NpcFactsBuilder::facts(), 0.2F) == 1);
    REQUIRE(simple_platformer::activeNpcMachineState(machine).name == "rest");
}

TEST_CASE("Among transitions from one state the first that holds wins", "[npc][fsm]")
{
    NpcMachine machine = simple_platformer::startNpcMachine(NpcMachineBuilder::named("test")
                                                                .state("rest", NpcState::Idle)
                                                                .state("hunt", NpcState::Chase)
                                                                .state("flee", NpcState::Retreat)
                                                                .transition("rest", "flee")
                                                                .when("targetTooClose", true)
                                                                .transition("rest", "hunt")
                                                                .when("targetKnown", true));

    // Both the flee and the hunt transitions hold; the flee is listed first.
    REQUIRE(
        simple_platformer::advanceNpcMachine(
            machine, NpcFactsBuilder::facts().targetTooClose(), 0.1F) == 0);
    REQUIRE(
        std::get<simple_platformer::BuiltInNpcActivity>(
            simple_platformer::activeNpcMachineState(machine).does)
            .state == NpcState::Retreat);
}

TEST_CASE("A machine that was not started cannot advance", "[npc][fsm][validation]")
{
    NpcMachine machine;
    machine.definition = restAndHunt();
    REQUIRE_THROWS_AS(
        simple_platformer::advanceNpcMachine(machine, NpcFactsBuilder::facts(), 0.1F),
        std::logic_error);
}
