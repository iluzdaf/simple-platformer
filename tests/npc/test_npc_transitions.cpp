#include <catch2/catch_test_macros.hpp>

#include <optional>

#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_transitions.hpp"
#include "support/npc_facts_builder.hpp"

using simple_platformer::nextNpcState;
using simple_platformer::NpcState;
using tests::NpcFactsBuilder;

TEST_CASE("An idle NPC patrols when it has a patrol and otherwise stays", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(NpcState::Idle, NpcFactsBuilder::facts().withPatrol()) == NpcState::Patrol);
    REQUIRE(nextNpcState(NpcState::Idle, NpcFactsBuilder::facts()) == std::nullopt);
}

TEST_CASE("A known target is chased from idle and from patrol", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(NpcState::Idle, NpcFactsBuilder::facts().withPatrol().knowingTarget()) ==
        NpcState::Chase);
    REQUIRE(
        nextNpcState(NpcState::Patrol, NpcFactsBuilder::facts().withPatrol().knowingTarget()) ==
        NpcState::Chase);
    REQUIRE(nextNpcState(NpcState::Patrol, NpcFactsBuilder::facts().withPatrol()) == std::nullopt);
}

TEST_CASE("A chase ends in patrol or idle once the target is lost", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(NpcState::Chase, NpcFactsBuilder::facts().withPatrol()) == NpcState::Patrol);
    REQUIRE(nextNpcState(NpcState::Chase, NpcFactsBuilder::facts()) == NpcState::Idle);
    REQUIRE(
        nextNpcState(NpcState::Chase, NpcFactsBuilder::facts().knowingTarget()) == std::nullopt);
}

TEST_CASE("A chase bites once the target is in range", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(NpcState::Chase, NpcFactsBuilder::facts().targetInBiteRange()) ==
        NpcState::Bite);
}

TEST_CASE("A bite holds until the bite is ready again after its entering update", "[npc][fsm]")
{
    REQUIRE(nextNpcState(NpcState::Bite, NpcFactsBuilder::facts().knowingTarget()) == std::nullopt);
    // Ready on the entering update means the bite has not started yet.
    REQUIRE(
        nextNpcState(NpcState::Bite, NpcFactsBuilder::facts().knowingTarget().biteReadyFor(0.0F)) ==
        std::nullopt);
}

TEST_CASE("A finished bite chases a known target and otherwise patrols or idles", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(NpcState::Bite, NpcFactsBuilder::facts().knowingTarget().biteReadyFor(0.1F)) ==
        NpcState::Chase);
    REQUIRE(
        nextNpcState(NpcState::Bite, NpcFactsBuilder::facts().withPatrol().biteReadyFor(0.1F)) ==
        NpcState::Patrol);
    REQUIRE(
        nextNpcState(NpcState::Bite, NpcFactsBuilder::facts().biteReadyFor(0.1F)) ==
        NpcState::Idle);
}
