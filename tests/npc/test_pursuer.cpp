#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/actor/actor_system.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_system.hpp"
#include "simple_platformer/npc/npc_behaviour.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"

#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/add_player.hpp"
#include "support/fixed_step.hpp"
#include "support/npc_facts_builder.hpp"
#include "support/tile_map_builder.hpp"

using simple_platformer::nextNpcState;
using simple_platformer::NpcState;
using tests::actor;
using tests::bite;
using tests::brain;
using tests::NpcFactsBuilder;
using tests::pathFollower;
using tests::patrol;

namespace
{
    constexpr simple_platformer::NpcTactic Pursuer = simple_platformer::NpcTactic::Pursuer;

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
        nextNpcState(Pursuer, NpcState::Patrol, NpcFactsBuilder::facts().targetInSights()) ==
        NpcState::Shoot);
}

TEST_CASE("A chase shoots a target in its sights and a shot chases one out of them", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(Pursuer, NpcState::Chase, NpcFactsBuilder::facts().targetInSights()) ==
        NpcState::Shoot);
    REQUIRE(
        nextNpcState(Pursuer, NpcState::Shoot, NpcFactsBuilder::facts().targetInSights()) ==
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
            NpcFactsBuilder::facts().targetInSights().targetInBiteRange()) == NpcState::Bite);
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

TEST_CASE("A Pursuer never retreats", "[npc][fsm]")
{
    REQUIRE(
        nextNpcState(
            Pursuer, NpcState::Chase, NpcFactsBuilder::facts().targetWithinStandoffDistance()) ==
        std::nullopt);
    REQUIRE(
        nextNpcState(
            Pursuer,
            NpcState::Idle,
            NpcFactsBuilder::facts().targetWithinStandoffDistance().targetInSights()) ==
        NpcState::Shoot);
}

TEST_CASE(
    "A searching NPC where the target was last known looks one way, then the other",
    "[npc][activities]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "########"});
    simple_platformer::World world;
    const simple_platformer::ActorId npcId = world.addActor(makeNpc({24.0F, 32.0F}));
    brain(world, npcId).state = simple_platformer::NpcState::Chase;
    brain(world, npcId).lastKnownTargetFeet = {24.0F, 32.0F};

    // Entering the search, then a turn's worth of looking.
    simple_platformer::updateNpcBehaviour(map, world, 0.3F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Search);
    REQUIRE(actor(world, npcId).intentions.aimDirection.x > 0.0F);
    simple_platformer::updateNpcBehaviour(map, world, 0.3F);
    REQUIRE(actor(world, npcId).intentions.aimDirection.x > 0.0F);

    simple_platformer::updateNpcBehaviour(map, world, 0.3F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Search);
    REQUIRE(actor(world, npcId).intentions.aimDirection.x < 0.0F);
}

TEST_CASE("A chasing NPC follows the last known target feet", "[npc][activities]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "########"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = tests::addPlayer(world, makePlayer({70.0F, 28.0F}));
    const simple_platformer::ActorId npcId = world.addActor(makeNpc({24.0F, 32.0F}));
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastKnownTargetFeet = {72.0F, 32.0F};
    tests::perception(world, npcId).targetVisible = false;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);

    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Chase);
    REQUIRE(actor(world, npcId).intentions.direction.x > 0.0F);
    // Following the path does not undo the aim at the target.
    REQUIRE(actor(world, npcId).intentions.aimDirection == glm::vec2{48.0F, 0.0F});
    REQUIRE_FALSE(actor(world, npcId).intentions.primaryAttackPressed);
}

TEST_CASE(
    "Ground pursuit resolves remembered feet without tracking the hidden player",
    "[npc][activities]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::World world;
    // The player is now to the right, but the last sighting was to the left.
    const auto playerId = world.addActor(makePlayer({70.0F, 32.0F}));
    // A walking NPC as tall as a zombie, standing on the floor at y = 32. Its height decides
    // where it can stand.
    const auto npcId = world.addActor(tests::ActorBuilder::sized({12.0F, 20.0F})
                                          .atFeet({56.0F, 32.0F})
                                          .platforming()
                                          .thinking({64.0F, 1.0F}));
    tests::platformerMovement(world, npcId).grounded = true;
    const glm::vec2 lastKnownFeet{8.0F, 20.0F};
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastKnownTargetFeet = lastKnownFeet;
    tests::perception(world, npcId).targetVisible = false;

    simple_platformer::updateNpcBehaviour(map, world, tests::FixedStepSeconds);

    REQUIRE(actor(world, npcId).intentions.direction.x < 0.0F);
    REQUIRE(pathFollower(world, npcId).goal == lastKnownFeet);
    REQUIRE(brain(world, npcId).lastKnownTargetFeet == lastKnownFeet);
}

TEST_CASE("An NPC enters bite once and returns to chase after recovery", "[npc][activities]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = tests::addPlayer(world, makePlayer({38.0F, 28.0F}));
    const simple_platformer::ActorId npcId =
        world.addActor(makeNpc({22.0F, 28.0F}).onTeam(simple_platformer::Team::Enemy).biting());
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastKnownTargetFeet = {38.0F, 28.0F};
    tests::perception(world, npcId).targetVisible = true;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Bite);
    REQUIRE(actor(world, npcId).intentions.primaryAttackPressed);

    simple_platformer::WorldRequests requests;
    simple_platformer::updateAttacks(world, requests, 0.1F);
    REQUIRE(bite(world, npcId).phase == simple_platformer::BitePhase::Windup);

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE_FALSE(actor(world, npcId).intentions.primaryAttackPressed);
    simple_platformer::updateAttacks(world, requests, 0.12F);
    REQUIRE(bite(world, npcId).phase == simple_platformer::BitePhase::Active);
    simple_platformer::updateAttacks(world, requests, 0.08F);
    REQUIRE(bite(world, npcId).phase == simple_platformer::BitePhase::Recovery);
    simple_platformer::updateAttacks(world, requests, 0.30F);
    REQUIRE(bite(world, npcId).phase == simple_platformer::BitePhase::Ready);

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Chase);
}

TEST_CASE("A ranged NPC shoots a visible target where it stands", "[npc][activities]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = tests::addPlayer(world, makePlayer({54.0F, 12.0F}));
    const simple_platformer::ActorId npcId =
        world.addActor(makeNpc({22.0F, 28.0F}).onTeam(simple_platformer::Team::Enemy).shooting());
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastKnownTargetFeet = {54.0F, 12.0F};
    tests::perception(world, npcId).targetVisible = true;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    // Facing is decided by the movement update from what the NPC intends.
    simple_platformer::updateActorMovement(map, world, 0.1F);

    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Shoot);
    REQUIRE(actor(world, npcId).facing == simple_platformer::Facing::Right);
    REQUIRE(actor(world, npcId).intentions.direction == glm::vec2{0.0F, 0.0F});
    REQUIRE(actor(world, npcId).intentions.aimDirection == glm::vec2{32.0F, -16.0F});
    REQUIRE(actor(world, npcId).intentions.primaryAttackPressed);
}

TEST_CASE("A patrol path produces intentions that move the flying NPC", "[npc][activities]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "........", "########"});
    simple_platformer::World world;
    tests::addPlayer(world, makePlayer({102.0F, 44.0F}));
    const simple_platformer::ActorId npcId =
        world.addActor(makeNpc({24.0F, 32.0F}).patrolling({24.0F, 32.0F}, {72.0F, 32.0F}));

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Patrol);
    REQUIRE(actor(world, npcId).intentions.direction.x > 0.0F);

    const float previousX = actor(world, npcId).body.bounds.topLeft.x;
    simple_platformer::updateActorMovement(map, world, 0.1F);
    REQUIRE(actor(world, npcId).body.bounds.topLeft.x > previousX);
}

TEST_CASE("A patrol swaps endpoints after reaching its goal", "[npc][activities]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::World world;
    tests::addPlayer(world, makePlayer({70.0F, 12.0F}));
    const simple_platformer::ActorId npcId =
        world.addActor(makeNpc({24.0F, 32.0F}).patrolling({24.0F, 32.0F}, {56.0F, 32.0F}));
    patrol(world, npcId).headingToSecond = false;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);

    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Patrol);
    REQUIRE(patrol(world, npcId).headingToSecond);
    REQUIRE_FALSE(pathFollower(world, npcId).path.has_value());
}

TEST_CASE("A chasing NPC searches for a lost target, then patrols again", "[npc][fsm]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"............", "............", "............", "############"});
    simple_platformer::World world;
    const simple_platformer::ActorId npcId =
        world.addActor(makeNpc({22.0F, 28.0F}).patrolling({24.0F, 32.0F}, {72.0F, 32.0F}));
    brain(world, npcId).state = simple_platformer::NpcState::Chase;
    brain(world, npcId).lastKnownTargetFeet = {22.0F, 28.0F};
    tests::senses(actor(world, npcId)).searchDuration = 0.25F;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Search);

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Search);

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Patrol);
}

TEST_CASE("A chasing NPC that does not search patrols again at once", "[npc][fsm]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"............", "............", "............", "############"});
    simple_platformer::World world;
    const simple_platformer::ActorId npcId =
        world.addActor(makeNpc({22.0F, 28.0F}).patrolling({24.0F, 32.0F}, {72.0F, 32.0F}));
    brain(world, npcId).state = simple_platformer::NpcState::Chase;
    tests::senses(actor(world, npcId)).searchDuration = 0.0F;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Patrol);
}

TEST_CASE("An NPC without a bite continues chasing at close range", "[npc][fsm]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = tests::addPlayer(world, makePlayer({38.0F, 28.0F}));
    const simple_platformer::ActorId npcId = world.addActor(makeNpc({22.0F, 28.0F}));
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastKnownTargetFeet = {38.0F, 28.0F};
    tests::perception(world, npcId).targetVisible = true;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);

    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Chase);
    REQUIRE_FALSE(actor(world, npcId).intentions.primaryAttackPressed);
    REQUIRE(actor(world, npcId).intentions.direction.x > 0.0F);
}
