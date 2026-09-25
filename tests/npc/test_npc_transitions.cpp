#include <catch2/catch_test_macros.hpp>

#include <optional>

#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_transitions.hpp"
#include "support/npc_facts_builder.hpp"

using simple_platformer::nextNpcState;
using simple_platformer::NpcState;
using tests::NpcFactsBuilder;

namespace
{
    constexpr simple_platformer::NpcTactic Pursuer = simple_platformer::NpcTactic::Pursuer;
    constexpr simple_platformer::NpcTactic KeepDistance =
        simple_platformer::NpcTactic::KeepDistance;
}

TEST_CASE("An idle NPC patrols when it has a patrol and otherwise stays", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(Pursuer, NpcState::Idle, NpcFactsBuilder::facts().withPatrol()) ==
        NpcState::Patrol);
    REQUIRE(nextNpcState(Pursuer, NpcState::Idle, NpcFactsBuilder::facts()) == std::nullopt);
}

TEST_CASE("A known target is chased from idle and from patrol", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(
            Pursuer, NpcState::Idle, NpcFactsBuilder::facts().withPatrol().knowingTarget()) ==
        NpcState::Chase);
    REQUIRE(
        nextNpcState(
            Pursuer, NpcState::Patrol, NpcFactsBuilder::facts().withPatrol().knowingTarget()) ==
        NpcState::Chase);
    REQUIRE(
        nextNpcState(Pursuer, NpcState::Patrol, NpcFactsBuilder::facts().withPatrol()) ==
        std::nullopt);
}

TEST_CASE(
    "A chase that does not search ends in patrol or idle once the target is lost",
    "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(Pursuer, NpcState::Chase, NpcFactsBuilder::facts().withPatrol()) ==
        NpcState::Patrol);
    REQUIRE(nextNpcState(Pursuer, NpcState::Chase, NpcFactsBuilder::facts()) == NpcState::Idle);
    REQUIRE(
        nextNpcState(Pursuer, NpcState::Chase, NpcFactsBuilder::facts().knowingTarget()) ==
        std::nullopt);
}

TEST_CASE("A chase bites once the target is in range", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(Pursuer, NpcState::Chase, NpcFactsBuilder::facts().targetInBiteRange()) ==
        NpcState::Bite);
}

TEST_CASE("A bite holds until the bite is ready again after its entering update", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(Pursuer, NpcState::Bite, NpcFactsBuilder::facts().knowingTarget()) ==
        std::nullopt);
    // Ready on the entering update means the bite has not started yet.
    REQUIRE(
        nextNpcState(
            Pursuer, NpcState::Bite, NpcFactsBuilder::facts().knowingTarget().biteReadyFor(0.0F)) ==
        std::nullopt);
}

TEST_CASE("A finished bite chases a known target and otherwise patrols or idles", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(
            Pursuer, NpcState::Bite, NpcFactsBuilder::facts().knowingTarget().biteReadyFor(0.1F)) ==
        NpcState::Chase);
    REQUIRE(
        nextNpcState(
            Pursuer, NpcState::Bite, NpcFactsBuilder::facts().withPatrol().biteReadyFor(0.1F)) ==
        NpcState::Patrol);
    REQUIRE(
        nextNpcState(Pursuer, NpcState::Bite, NpcFactsBuilder::facts().biteReadyFor(0.1F)) ==
        NpcState::Idle);
}

TEST_CASE("A target in reach is attacked straight from idle or patrol", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(Pursuer, NpcState::Idle, NpcFactsBuilder::facts().targetInBiteRange()) ==
        NpcState::Bite);
    REQUIRE(
        nextNpcState(Pursuer, NpcState::Patrol, NpcFactsBuilder::facts().canShootTarget()) ==
        NpcState::Shoot);
}

TEST_CASE("A chase shoots a target in its sights and a shot chases one out of them", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(Pursuer, NpcState::Chase, NpcFactsBuilder::facts().canShootTarget()) ==
        NpcState::Shoot);
    REQUIRE(
        nextNpcState(Pursuer, NpcState::Shoot, NpcFactsBuilder::facts().canShootTarget()) ==
        std::nullopt);
    REQUIRE(
        nextNpcState(Pursuer, NpcState::Shoot, NpcFactsBuilder::facts().knowingTarget()) ==
        NpcState::Chase);
}

TEST_CASE("A shot ends in patrol or idle once the target is lost", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(Pursuer, NpcState::Shoot, NpcFactsBuilder::facts().withPatrol()) ==
        NpcState::Patrol);
    REQUIRE(nextNpcState(Pursuer, NpcState::Shoot, NpcFactsBuilder::facts()) == NpcState::Idle);
}

TEST_CASE("A bite comes before a shot at a target in range of both", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(
            Pursuer,
            NpcState::Shoot,
            NpcFactsBuilder::facts().canShootTarget().targetInBiteRange()) == NpcState::Bite);
}

TEST_CASE("A searcher searches for a lost target from a chase, a shot or a bite", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(Pursuer, NpcState::Chase, NpcFactsBuilder::facts().searching()) ==
        NpcState::Search);
    REQUIRE(
        nextNpcState(Pursuer, NpcState::Shoot, NpcFactsBuilder::facts().searching()) ==
        NpcState::Search);
    REQUIRE(
        nextNpcState(
            Pursuer, NpcState::Bite, NpcFactsBuilder::facts().searching().biteReadyFor(0.1F)) ==
        NpcState::Search);
}

TEST_CASE("A search pursues a target found again", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(
            Pursuer, NpcState::Search, NpcFactsBuilder::facts().searching().knowingTarget()) ==
        NpcState::Chase);
    REQUIRE(
        nextNpcState(
            Pursuer, NpcState::Search, NpcFactsBuilder::facts().searching().targetInBiteRange()) ==
        NpcState::Bite);
}

TEST_CASE("A search ends in patrol or idle once its time is up", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(Pursuer, NpcState::Search, NpcFactsBuilder::facts().searching()) ==
        std::nullopt);
    REQUIRE(
        nextNpcState(
            Pursuer, NpcState::Search, NpcFactsBuilder::facts().searchTimeUp().withPatrol()) ==
        NpcState::Patrol);
    REQUIRE(
        nextNpcState(Pursuer, NpcState::Search, NpcFactsBuilder::facts().searchTimeUp()) ==
        NpcState::Idle);
}

TEST_CASE("A KeepDistance NPC retreats from a target that has come too close", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(KeepDistance, NpcState::Idle, NpcFactsBuilder::facts().targetTooClose()) ==
        NpcState::Retreat);
    REQUIRE(
        nextNpcState(KeepDistance, NpcState::Chase, NpcFactsBuilder::facts().targetTooClose()) ==
        NpcState::Retreat);
    REQUIRE(
        nextNpcState(
            KeepDistance,
            NpcState::Shoot,
            NpcFactsBuilder::facts().targetTooClose().canShootTarget()) == NpcState::Retreat);
    REQUIRE(
        nextNpcState(
            KeepDistance,
            NpcState::Search,
            NpcFactsBuilder::facts().searching().targetTooClose()) == NpcState::Retreat);
}

TEST_CASE(
    "A retreat shoots or chases once the target is far enough, and watches once it is lost",
    "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(KeepDistance, NpcState::Retreat, NpcFactsBuilder::facts().targetTooClose()) ==
        std::nullopt);
    REQUIRE(
        nextNpcState(KeepDistance, NpcState::Retreat, NpcFactsBuilder::facts().canShootTarget()) ==
        NpcState::Shoot);
    REQUIRE(
        nextNpcState(KeepDistance, NpcState::Retreat, NpcFactsBuilder::facts().knowingTarget()) ==
        NpcState::Chase);
    REQUIRE(
        nextNpcState(KeepDistance, NpcState::Retreat, NpcFactsBuilder::facts().searching()) ==
        NpcState::Watch);
}

TEST_CASE("A Pursuer never retreats", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(Pursuer, NpcState::Chase, NpcFactsBuilder::facts().targetTooClose()) ==
        std::nullopt);
    REQUIRE(
        nextNpcState(
            Pursuer, NpcState::Idle, NpcFactsBuilder::facts().targetTooClose().canShootTarget()) ==
        NpcState::Shoot);
}

TEST_CASE("A KeepDistance NPC that loses its target watches from where it stands", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(KeepDistance, NpcState::Chase, NpcFactsBuilder::facts().searching()) ==
        NpcState::Watch);
    REQUIRE(
        nextNpcState(KeepDistance, NpcState::Retreat, NpcFactsBuilder::facts().searching()) ==
        NpcState::Watch);
    REQUIRE(
        nextNpcState(
            KeepDistance,
            NpcState::Bite,
            NpcFactsBuilder::facts().searching().biteReadyFor(0.1F)) == NpcState::Watch);
    REQUIRE(
        nextNpcState(KeepDistance, NpcState::Chase, NpcFactsBuilder::facts().withPatrol()) ==
        NpcState::Patrol);
}

TEST_CASE(
    "A watch pursues a target found again and otherwise ends when its time is up",
    "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(KeepDistance, NpcState::Watch, NpcFactsBuilder::facts().searching()) ==
        std::nullopt);
    REQUIRE(
        nextNpcState(
            KeepDistance, NpcState::Watch, NpcFactsBuilder::facts().searching().canShootTarget()) ==
        NpcState::Shoot);
    REQUIRE(
        nextNpcState(
            KeepDistance, NpcState::Watch, NpcFactsBuilder::facts().searching().targetTooClose()) ==
        NpcState::Retreat);
    REQUIRE(
        nextNpcState(
            KeepDistance, NpcState::Watch, NpcFactsBuilder::facts().searchTimeUp().withPatrol()) ==
        NpcState::Patrol);
    REQUIRE(
        nextNpcState(KeepDistance, NpcState::Watch, NpcFactsBuilder::facts().searchTimeUp()) ==
        NpcState::Idle);
}
