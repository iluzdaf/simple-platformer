#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_system.hpp"
#include "simple_platformer/npc/npc_behaviour.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/add_player.hpp"
#include "support/npc_facts_builder.hpp"
#include "support/tile_map_builder.hpp"

using simple_platformer::nextNpcState;
using simple_platformer::NpcState;
using tests::actor;
using tests::brain;
using tests::NpcFactsBuilder;
using tests::pathFollower;

namespace
{
    constexpr simple_platformer::NpcTactic KeepDistance =
        simple_platformer::NpcTactic::KeepDistance;

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

TEST_CASE("A KeepDistance NPC retreats from a target that has come too close", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(
            KeepDistance,
            NpcState::Idle,
            NpcFactsBuilder::facts().targetWithinStandoffDistance()) == NpcState::Retreat);
    REQUIRE(
        nextNpcState(
            KeepDistance,
            NpcState::Chase,
            NpcFactsBuilder::facts().targetWithinStandoffDistance()) == NpcState::Retreat);
    REQUIRE(
        nextNpcState(
            KeepDistance,
            NpcState::Shoot,
            NpcFactsBuilder::facts().targetWithinStandoffDistance().targetInSights()) ==
        NpcState::Retreat);
    REQUIRE(
        nextNpcState(
            KeepDistance,
            NpcState::Search,
            NpcFactsBuilder::facts().searching().targetWithinStandoffDistance()) ==
        NpcState::Retreat);
}

TEST_CASE(
    "A retreat shoots or chases once the target is far enough, and watches once it is lost",
    "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(
            KeepDistance,
            NpcState::Retreat,
            NpcFactsBuilder::facts().targetWithinStandoffDistance()) == std::nullopt);
    REQUIRE(
        nextNpcState(KeepDistance, NpcState::Retreat, NpcFactsBuilder::facts().targetInSights()) ==
        NpcState::Shoot);
    REQUIRE(
        nextNpcState(KeepDistance, NpcState::Retreat, NpcFactsBuilder::facts().knowingTarget()) ==
        NpcState::Chase);
    REQUIRE(
        nextNpcState(KeepDistance, NpcState::Retreat, NpcFactsBuilder::facts().searching()) ==
        NpcState::Watch);
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
            KeepDistance, NpcState::Watch, NpcFactsBuilder::facts().searching().targetInSights()) ==
        NpcState::Shoot);
    REQUIRE(
        nextNpcState(
            KeepDistance,
            NpcState::Watch,
            NpcFactsBuilder::facts().searching().targetWithinStandoffDistance()) ==
        NpcState::Retreat);
    REQUIRE(
        nextNpcState(
            KeepDistance, NpcState::Watch, NpcFactsBuilder::facts().searchTimeUp().withPatrol()) ==
        NpcState::Patrol);
    REQUIRE(
        nextNpcState(KeepDistance, NpcState::Watch, NpcFactsBuilder::facts().searchTimeUp()) ==
        NpcState::Idle);
}

TEST_CASE(
    "A KeepDistance walker backs away from a close target, firing, and holds at a ledge",
    "[npc][activities]")
{
    // Ground under columns 2 to 5 only.
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "..####.."});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = tests::addPlayer(world, makePlayer({40.0F, 32.0F}));
    const simple_platformer::ActorId npcId =
        world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                           .atFeet({72.0F, 32.0F})
                           .platforming()
                           .onTeam(simple_platformer::Team::Enemy)
                           .shooting()
                           .thinking({96.0F, 1.0F}));
    brain(world, npcId).tactic = simple_platformer::NpcTactic::KeepDistance;
    tests::senses(actor(world, npcId)).standoffDistance = 64.0F;
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastKnownTargetFeet = {40.0F, 32.0F};
    tests::perception(world, npcId).targetVisible = true;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Retreat);
    REQUIRE(actor(world, npcId).intentions.direction.x > 0.0F);
    REQUIRE(actor(world, npcId).intentions.aimDirection.x < 0.0F);
    REQUIRE(actor(world, npcId).intentions.primaryAttackPressed);

    // A body width from the ledge, the next cell along cannot be stood on.
    simple_platformer::moveFeetTo(actor(world, npcId).body.bounds, {88.0F, 32.0F});
    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Retreat);
    REQUIRE(actor(world, npcId).intentions.direction.x == 0.0F);
    REQUIRE(actor(world, npcId).intentions.primaryAttackPressed);
}

TEST_CASE("A watching NPC looks about without leaving where it stands", "[npc][activities]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "########"});
    simple_platformer::World world;
    const simple_platformer::ActorId npcId = world.addActor(makeNpc({24.0F, 32.0F}));
    brain(world, npcId).tactic = simple_platformer::NpcTactic::KeepDistance;
    brain(world, npcId).state = simple_platformer::NpcState::Chase;
    brain(world, npcId).lastKnownTargetFeet = {72.0F, 32.0F};

    simple_platformer::updateNpcBehaviour(map, world, 0.3F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Watch);
    REQUIRE(actor(world, npcId).intentions.direction == glm::vec2{0.0F, 0.0F});
    REQUIRE(actor(world, npcId).intentions.aimDirection.x > 0.0F);
    REQUIRE_FALSE(pathFollower(world, npcId).path.has_value());

    simple_platformer::updateNpcBehaviour(map, world, 0.3F);
    simple_platformer::updateNpcBehaviour(map, world, 0.3F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Watch);
    REQUIRE(actor(world, npcId).intentions.direction == glm::vec2{0.0F, 0.0F});
    REQUIRE(actor(world, npcId).intentions.aimDirection.x < 0.0F);
}

TEST_CASE("A KeepDistance NPC shoots once its target is at its standoff", "[npc][fsm]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "########"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = tests::addPlayer(world, makePlayer({24.0F, 32.0F}));
    const simple_platformer::ActorId npcId =
        world.addActor(makeNpc({88.0F, 32.0F}).onTeam(simple_platformer::Team::Enemy).shooting());
    brain(world, npcId).tactic = simple_platformer::NpcTactic::KeepDistance;
    tests::senses(actor(world, npcId)).standoffDistance = 48.0F;
    brain(world, npcId).state = simple_platformer::NpcState::Retreat;
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastKnownTargetFeet = {24.0F, 32.0F};
    tests::perception(world, npcId).targetVisible = true;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Shoot);
    REQUIRE(actor(world, npcId).intentions.direction == glm::vec2{0.0F, 0.0F});
    REQUIRE(actor(world, npcId).intentions.primaryAttackPressed);
}
