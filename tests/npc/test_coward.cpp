#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_facts.hpp"
#include "simple_platformer/npc/npc_system.hpp"
#include "simple_platformer/npc/npc_behaviour.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"

#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/add_player.hpp"
#include "support/fixed_step.hpp"
#include "support/tile_map_builder.hpp"

using simple_platformer::nextNpcState;
using simple_platformer::NpcState;
using tests::actor;
using tests::bite;
using tests::brain;
using tests::pathFollower;
using tests::patrol;

namespace
{
    tests::ActorBuilder makePlayer(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F}).atFeet(feet).platforming();
    }

    tests::ActorBuilder::Thinking makeNpc(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F})
            .atFeet(feet)
            .flying(20.0F)
            .thinking({64.0F, 1.0F});
    }
}

TEST_CASE("A Coward flees close visible threats and bites only while fleeing", "[npc][fsm][coward]")
{
    const auto tactic = simple_platformer::NpcTactic::Coward;
    simple_platformer::NpcFacts facts;
    facts.hasPatrol = true;
    facts.targetKnown = true;
    REQUIRE(nextNpcState(tactic, NpcState::Idle, facts) == NpcState::Patrol);
    REQUIRE(nextNpcState(tactic, NpcState::Patrol, facts) == std::nullopt);
    facts.targetVisible = true;
    REQUIRE(nextNpcState(tactic, NpcState::Patrol, facts) == std::nullopt);
    facts.targetWithinStandoffDistance = true;
    REQUIRE(nextNpcState(tactic, NpcState::Patrol, facts) == NpcState::Flee);
    facts.targetInBiteRange = true;
    REQUIRE(nextNpcState(tactic, NpcState::Flee, facts) == NpcState::Bite);
    facts.biteReady = true;
    REQUIRE(nextNpcState(tactic, NpcState::Bite, facts) == std::nullopt);
    facts.stateElapsed = 0.1F;
    facts.biteReady = false;
    REQUIRE(nextNpcState(tactic, NpcState::Bite, facts) == std::nullopt);
    facts.biteReady = true;
    REQUIRE(nextNpcState(tactic, NpcState::Bite, facts) == NpcState::Flee);
    facts.targetKnown = false;
    REQUIRE(nextNpcState(tactic, NpcState::Bite, facts) == NpcState::Patrol);
}

TEST_CASE(
    "A Coward waits for continuous target loss before resuming its routine",
    "[npc][fsm][coward]")
{
    const auto tactic = simple_platformer::NpcTactic::Coward;
    simple_platformer::NpcFacts facts;
    facts.hasPatrol = true;
    facts.stateElapsed = 10.0F;
    facts.targetLostElapsed = 1.4F;
    REQUIRE(nextNpcState(tactic, NpcState::Flee, facts) == std::nullopt);
    facts.targetLostElapsed = 1.5F;
    REQUIRE(nextNpcState(tactic, NpcState::Flee, facts) == NpcState::Patrol);
    facts.hasPatrol = false;
    REQUIRE(nextNpcState(tactic, NpcState::Flee, facts) == NpcState::Idle);
}

TEST_CASE(
    "A Coward chooses the refuge away from the threat and faces it when cornered",
    "[npc][activities][coward]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "..........", "##########"});
    simple_platformer::World world;
    const auto playerId = tests::addPlayer(world, makePlayer({72.0F, 32.0F}));
    const auto npcId = world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                                          .atFeet({56.0F, 32.0F})
                                          .platforming()
                                          .onTeam(simple_platformer::Team::Enemy)
                                          .thinking({})
                                          .patrolling({104.0F, 32.0F}, {24.0F, 32.0F})
                                          .biting());
    tests::platformerMovement(world, npcId).grounded = true;
    brain(world, npcId).tactic = simple_platformer::NpcTactic::Coward;
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastKnownTargetFeet = {72.0F, 32.0F};
    tests::perception(world, npcId).targetVisible = true;
    simple_platformer::updateNpcBehaviour(map, world, tests::FixedStepSeconds);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Flee);
    REQUIRE(pathFollower(world, npcId).goal == glm::vec2{24.0F, 32.0F});
    REQUIRE(actor(world, npcId).intentions.direction.x < 0.0F);
    REQUIRE_FALSE(actor(world, npcId).intentions.primaryAttackPressed);

    // A threat on the other side selects the other endpoint, even with reversed patrol ends.
    simple_platformer::moveFeetTo(actor(world, playerId).body.bounds, {8.0F, 32.0F});
    brain(world, npcId).lastKnownTargetFeet = {8.0F, 32.0F};
    simple_platformer::updateNpcBehaviour(map, world, tests::FixedStepSeconds);
    REQUIRE(pathFollower(world, npcId).goal == glm::vec2{104.0F, 32.0F});
    REQUIRE(actor(world, npcId).intentions.direction.x > 0.0F);
    simple_platformer::moveFeetTo(actor(world, playerId).body.bounds, {72.0F, 32.0F});
    brain(world, npcId).lastKnownTargetFeet = {72.0F, 32.0F};

    simple_platformer::moveFeetTo(actor(world, npcId).body.bounds, {24.0F, 32.0F});
    simple_platformer::updateNpcBehaviour(map, world, tests::FixedStepSeconds);
    REQUIRE_FALSE(pathFollower(world, npcId).path.has_value());
    REQUIRE(actor(world, npcId).intentions.direction == glm::vec2{});
    REQUIRE(actor(world, npcId).intentions.aimDirection.x > 0.0F);

    // Once the player closes on the corner, the bite is requested only on entry.
    simple_platformer::moveFeetTo(actor(world, playerId).body.bounds, {36.0F, 32.0F});
    brain(world, npcId).lastKnownTargetFeet = {36.0F, 32.0F};
    actor(world, npcId).facing = simple_platformer::Facing::Right;
    simple_platformer::updateNpcBehaviour(map, world, tests::FixedStepSeconds);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Bite);
    REQUIRE(actor(world, npcId).intentions.primaryAttackPressed);
    simple_platformer::WorldRequests requests;
    simple_platformer::updateAttacks(world, requests, tests::FixedStepSeconds);
    simple_platformer::updateNpcBehaviour(map, world, tests::FixedStepSeconds);
    REQUIRE_FALSE(actor(world, npcId).intentions.primaryAttackPressed);
}

TEST_CASE("A Coward resets its loss timer when the target returns", "[npc][activities][coward]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "..........", "##########"});
    simple_platformer::World world;
    const auto targetId = tests::addPlayer(world, makePlayer({104.0F, 32.0F}));
    const auto npcId =
        world.addActor(makeNpc({56.0F, 32.0F}).patrolling({24.0F, 32.0F}, {104.0F, 32.0F}));
    brain(world, npcId).tactic = simple_platformer::NpcTactic::Coward;
    brain(world, npcId).state = simple_platformer::NpcState::Flee;
    brain(world, npcId).stateElapsed = 10.0F;
    simple_platformer::updateNpcBehaviour(map, world, 1.0F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Flee);
    brain(world, npcId).target = targetId;
    brain(world, npcId).lastKnownTargetFeet = {104.0F, 32.0F};
    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).targetLostElapsed == 0.0F);
    brain(world, npcId).target.reset();
    simple_platformer::updateNpcBehaviour(map, world, 1.0F);
    simple_platformer::updateNpcBehaviour(map, world, 0.5F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Flee);
    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Patrol);
}
