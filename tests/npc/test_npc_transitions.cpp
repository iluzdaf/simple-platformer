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

TEST_CASE(
    "A chase that does not search ends in patrol or idle once the target is lost",
    "[npc][fsm]")
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

TEST_CASE("A target in reach is attacked straight from idle or patrol", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(NpcState::Idle, NpcFactsBuilder::facts().targetInBiteRange()) ==
        NpcState::Bite);
    REQUIRE(
        nextNpcState(NpcState::Patrol, NpcFactsBuilder::facts().canShootTarget()) ==
        NpcState::Shoot);
}

TEST_CASE("A chase shoots a target in its sights and a shot chases one out of them", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(NpcState::Chase, NpcFactsBuilder::facts().canShootTarget()) ==
        NpcState::Shoot);
    REQUIRE(
        nextNpcState(NpcState::Shoot, NpcFactsBuilder::facts().canShootTarget()) == std::nullopt);
    REQUIRE(
        nextNpcState(NpcState::Shoot, NpcFactsBuilder::facts().knowingTarget()) == NpcState::Chase);
}

TEST_CASE("A shot ends in patrol or idle once the target is lost", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(NpcState::Shoot, NpcFactsBuilder::facts().withPatrol()) == NpcState::Patrol);
    REQUIRE(nextNpcState(NpcState::Shoot, NpcFactsBuilder::facts()) == NpcState::Idle);
}

TEST_CASE("A bite comes before a shot at a target in range of both", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(
            NpcState::Shoot, NpcFactsBuilder::facts().canShootTarget().targetInBiteRange()) ==
        NpcState::Bite);
}

TEST_CASE("A searcher searches for a lost target from a chase, a shot or a bite", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(NpcState::Chase, NpcFactsBuilder::facts().searching()) == NpcState::Search);
    REQUIRE(
        nextNpcState(NpcState::Shoot, NpcFactsBuilder::facts().searching()) == NpcState::Search);
    REQUIRE(
        nextNpcState(NpcState::Bite, NpcFactsBuilder::facts().searching().biteReadyFor(0.1F)) ==
        NpcState::Search);
}

TEST_CASE("A search pursues a target found again", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(NpcState::Search, NpcFactsBuilder::facts().searching().knowingTarget()) ==
        NpcState::Chase);
    REQUIRE(
        nextNpcState(NpcState::Search, NpcFactsBuilder::facts().searching().targetInBiteRange()) ==
        NpcState::Bite);
}

TEST_CASE("A search ends in patrol or idle once its time is up", "[npc][fsm]")
{
    REQUIRE(nextNpcState(NpcState::Search, NpcFactsBuilder::facts().searching()) == std::nullopt);
    REQUIRE(
        nextNpcState(NpcState::Search, NpcFactsBuilder::facts().searchTimeUp().withPatrol()) ==
        NpcState::Patrol);
    REQUIRE(
        nextNpcState(NpcState::Search, NpcFactsBuilder::facts().searchTimeUp()) == NpcState::Idle);
}
