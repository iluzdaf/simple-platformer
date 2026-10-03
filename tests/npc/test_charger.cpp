#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_system.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_facts.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/npc/npc_system.hpp"
#include "simple_platformer/npc/npc_behaviour.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/add_player.hpp"
#include "support/tile_map_builder.hpp"

using simple_platformer::nextNpcState;
using simple_platformer::NpcState;
using tests::actor;
using tests::brain;

namespace
{
    tests::ActorBuilder makePlayer(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F}).atFeet(feet).platforming();
    }
}

TEST_CASE("A Charger wakes only for a nearby landing on its ground run", "[npc][fsm][charger]")
{
    const auto tactic = simple_platformer::NpcTactic::Charger;
    simple_platformer::NpcFacts facts;
    facts.targetKnown = true;
    facts.targetVisible = true;
    REQUIRE(nextNpcState(tactic, NpcState::Idle, facts) == NpcState::Sleep);
    REQUIRE(nextNpcState(tactic, NpcState::Sleep, facts) == std::nullopt);
    facts.heardLanding = true;
    facts.targetOnSameRun = true;
    REQUIRE(nextNpcState(tactic, NpcState::Sleep, facts) == std::nullopt);
    facts.targetWithinNoticeDistance = true;
    REQUIRE(nextNpcState(tactic, NpcState::Sleep, facts) == NpcState::Charge);
    facts.targetOnSameRun = false;
    REQUIRE(nextNpcState(tactic, NpcState::Sleep, facts) == std::nullopt);
}

TEST_CASE(
    "A Charger commits until blocked and recovers before charging or sleeping",
    "[npc][fsm][charger]")
{
    const auto tactic = simple_platformer::NpcTactic::Charger;
    simple_platformer::NpcFacts facts;
    REQUIRE(nextNpcState(tactic, NpcState::Charge, facts) == std::nullopt);
    facts.movementBlocked = true;
    REQUIRE(nextNpcState(tactic, NpcState::Charge, facts) == NpcState::Stunned);
    facts.stateElapsed = 1.4F;
    REQUIRE(nextNpcState(tactic, NpcState::Stunned, facts) == std::nullopt);
    facts.stateElapsed = 1.5F;
    REQUIRE(nextNpcState(tactic, NpcState::Stunned, facts) == NpcState::Sleep);
    facts.targetOnSameRun = true;
    facts.targetWithinNoticeDistance = true;
    REQUIRE(nextNpcState(tactic, NpcState::Stunned, facts) == NpcState::Charge);
    facts.targetWithinNoticeDistance = false;
    REQUIRE(nextNpcState(tactic, NpcState::Stunned, facts) == NpcState::Sleep);
}

TEST_CASE(
    "A Charger wakes on landing, keeps its direction and stops contact damage when blocked",
    "[npc][activities][charger]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", ".....#....", "##########"});
    simple_platformer::World world;
    const auto playerId = tests::addPlayer(world, makePlayer({56.0F, 32.0F}));
    actor(world, playerId).team = simple_platformer::Team::Player;
    const auto npcId = world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                                          .atFeet({24.0F, 32.0F})
                                          .platforming()
                                          .onTeam(simple_platformer::Team::Enemy)
                                          .thinking({80.0F, 1.0F})
                                          .withContactDamage());
    brain(world, npcId).tactic = simple_platformer::NpcTactic::Charger;
    tests::platformerMovement(actor(world, npcId)).grounded = true;
    tests::platformerMovement(actor(world, playerId)).grounded = true;
    simple_platformer::updateNpcSenses(map, world, 0.1F);
    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Sleep);
    REQUIRE_FALSE(actor(world, npcId).intentions.contactDamage);
    world.emitNoise({playerId, {56.0F, 32.0F}, simple_platformer::NoiseKind::Landing});
    simple_platformer::updateNpcSenses(map, world, 0.1F);
    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Charge);
    REQUIRE(actor(world, npcId).intentions.direction.x == 1.0F);
    REQUIRE(actor(world, npcId).intentions.contactDamage);
    REQUIRE(actor(world, npcId).intentions.avoidLedges);
    // Moving the target behind the boar cannot reverse an active charge.
    simple_platformer::moveFeetTo(actor(world, playerId).body.bounds, {8.0F, 32.0F});
    simple_platformer::updateNpcSenses(map, world, 0.1F);
    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(actor(world, npcId).intentions.direction.x == 1.0F);
    for (int tick = 0; tick < 20 && !tests::platformerMovement(actor(world, npcId)).blocked; ++tick)
    {
        simple_platformer::updateActorMovement(map, world, 0.1F);
    }
    REQUIRE(tests::platformerMovement(actor(world, npcId)).blocked);
    simple_platformer::updateNpcSenses(map, world, 0.1F);
    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Stunned);
    REQUIRE_FALSE(actor(world, npcId).intentions.contactDamage);
    REQUIRE(actor(world, npcId).intentions.direction == glm::vec2{});
    brain(world, npcId).stateElapsed = 1.5F;
    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Charge);
    REQUIRE(actor(world, npcId).intentions.direction.x == -1.0F);
}
