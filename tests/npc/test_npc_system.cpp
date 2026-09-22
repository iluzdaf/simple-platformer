#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/actor/actor_system.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_system.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"
#include "support/require_near.hpp"
#include "support/tile_map_builder.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"

using tests::actor;
using tests::bite;
using tests::brain;
using tests::pathFollower;
using tests::patrol;

namespace
{
    // An actor the world can treat as the player.
    tests::ActorBuilder makePlayer(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F}).atFeet(feet).walking();
    }

    // The NPC these tests measure their maps against.
    tests::ActorBuilder makeNpc(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F})
            .atFeet(feet)
            .flying(20.0F)
            .thinking({64.0F, 1.0F});
    }
}

TEST_CASE("NPC behaviour rejects invalid timing", "[npc][validation]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"...", "...", "###"});
    simple_platformer::World world;

    REQUIRE_THROWS_AS(
        simple_platformer::updateNpcBehaviour(map, world, -0.1F), std::invalid_argument);
}

TEST_CASE("A chasing NPC patrols again once it has no target", "[npc][fsm]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"............", "............", "............", "############"});
    simple_platformer::World world;
    const simple_platformer::ActorId npcId =
        world.addActor(makeNpc({22.0F, 28.0F}).patrolling({24.0F, 32.0F}, {72.0F, 32.0F}));
    brain(world, npcId).state = simple_platformer::NpcState::Chase;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Patrol);
}

TEST_CASE("A chasing NPC follows the last seen target feet", "[npc][fsm]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "########"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = world.addActor(makePlayer({70.0F, 28.0F}));
    world.setPlayer(playerId, {70.0F, 28.0F});
    const simple_platformer::ActorId npcId = world.addActor(makeNpc({24.0F, 32.0F}));
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastSeenTargetFeet = {72.0F, 32.0F};
    brain(world, npcId).targetVisible = false;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);

    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Chase);
    REQUIRE(actor(world, npcId).intentions.direction.x > 0.0F);
    REQUIRE_FALSE(actor(world, npcId).intentions.primaryAttackPressed);
}

TEST_CASE(
    "Ground pursuit resolves remembered feet without tracking the hidden player",
    "[npc][fsm]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::World world;
    // The player is now to the right, but the last sighting was to the left.
    const auto playerId = world.addActor(makePlayer({70.0F, 32.0F}));
    // A walking NPC as tall as a zombie, standing on the floor at y = 32. Its height decides
    // where it can stand.
    const auto npcId = world.addActor(tests::ActorBuilder::sized({12.0F, 20.0F})
                                          .atFeet({56.0F, 32.0F})
                                          .walking()
                                          .thinking({64.0F, 1.0F}));
    tests::platformerMovement(world, npcId).grounded = true;
    const glm::vec2 lastSeenFeet{8.0F, 20.0F};
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastSeenTargetFeet = lastSeenFeet;
    brain(world, npcId).targetVisible = false;

    simple_platformer::updateNpcBehaviour(map, world, 1.0F / 60.0F);

    REQUIRE(actor(world, npcId).intentions.direction.x < 0.0F);
    REQUIRE(pathFollower(world, npcId).destination == simple_platformer::GridPosition{0, 1});
    REQUIRE(brain(world, npcId).lastSeenTargetFeet == lastSeenFeet);
}

TEST_CASE("An NPC enters bite once and returns to chase after recovery", "[npc][fsm]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = world.addActor(makePlayer({38.0F, 28.0F}));
    world.setPlayer(playerId, {38.0F, 28.0F});
    const simple_platformer::ActorId npcId =
        world.addActor(makeNpc({22.0F, 28.0F}).onTeam(simple_platformer::Team::Enemy).biting());
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastSeenTargetFeet = {38.0F, 28.0F};
    brain(world, npcId).targetVisible = true;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Bite);
    REQUIRE(actor(world, npcId).intentions.primaryAttackPressed);

    simple_platformer::WorldRequests requests;
    simple_platformer::updateAttacks(world, requests, 0.1F);
    REQUIRE(bite(world, npcId).phase == simple_platformer::BitePhase::Windup);

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE_FALSE(actor(world, npcId).intentions.primaryAttackPressed);
    simple_platformer::updateAttacks(world, requests, 1.0F);
    REQUIRE(bite(world, npcId).phase == simple_platformer::BitePhase::Ready);

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Chase);
}

TEST_CASE("An NPC without a bite continues chasing at close range", "[npc][fsm]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = world.addActor(makePlayer({38.0F, 28.0F}));
    world.setPlayer(playerId, {38.0F, 28.0F});
    const simple_platformer::ActorId npcId = world.addActor(makeNpc({22.0F, 28.0F}));
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastSeenTargetFeet = {38.0F, 28.0F};
    brain(world, npcId).targetVisible = true;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);

    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Chase);
    REQUIRE_FALSE(actor(world, npcId).intentions.primaryAttackPressed);
    REQUIRE(actor(world, npcId).intentions.direction.x > 0.0F);
}

TEST_CASE("A ranged NPC stops and requests an attack while its target is visible", "[npc][fsm]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = world.addActor(makePlayer({54.0F, 12.0F}));
    world.setPlayer(playerId, {54.0F, 12.0F});
    const simple_platformer::ActorId npcId =
        world.addActor(makeNpc({22.0F, 28.0F}).onTeam(simple_platformer::Team::Enemy).shooting());
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastSeenTargetFeet = {54.0F, 12.0F};
    brain(world, npcId).targetVisible = true;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);

    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Chase);
    REQUIRE(actor(world, npcId).facing == simple_platformer::Facing::Right);
    REQUIRE(actor(world, npcId).intentions.direction == glm::vec2{0.0F, 0.0F});
    REQUIRE(actor(world, npcId).intentions.aimDirection == glm::vec2{32.0F, -16.0F});
    REQUIRE(actor(world, npcId).intentions.primaryAttackPressed);
}

TEST_CASE("A patrol path produces intentions that move the flying NPC", "[npc][fsm]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "........", "########"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = world.addActor(makePlayer({102.0F, 44.0F}));
    world.setPlayer(playerId, {102.0F, 44.0F});
    const simple_platformer::ActorId npcId =
        world.addActor(makeNpc({24.0F, 32.0F}).patrolling({24.0F, 32.0F}, {72.0F, 32.0F}));

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Patrol);
    REQUIRE(actor(world, npcId).intentions.direction.x > 0.0F);

    const float previousX = actor(world, npcId).body.bounds.position.x;
    simple_platformer::updateActorMovement(map, world, 0.1F);
    REQUIRE(actor(world, npcId).body.bounds.position.x > previousX);
}

TEST_CASE("A patrol swaps endpoints after reaching its destination", "[npc][fsm]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = world.addActor(makePlayer({70.0F, 12.0F}));
    world.setPlayer(playerId, {70.0F, 12.0F});
    const simple_platformer::ActorId npcId =
        world.addActor(makeNpc({24.0F, 32.0F}).patrolling({24.0F, 32.0F}, {56.0F, 32.0F}));
    patrol(world, npcId).headingToSecond = false;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);

    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Patrol);
    REQUIRE(patrol(world, npcId).headingToSecond);
    REQUIRE_FALSE(pathFollower(world, npcId).path.has_value());
}

TEST_CASE("An unreachable patrol waits before retrying its path", "[npc][fsm]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"....#....", "....#....", "#########"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = world.addActor(makePlayer({22.0F, 12.0F}));
    world.setPlayer(playerId, {22.0F, 12.0F});
    const simple_platformer::ActorId npcId =
        world.addActor(makeNpc({24.0F, 32.0F}).patrolling({24.0F, 32.0F}, {120.0F, 32.0F}));

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE_FALSE(pathFollower(world, npcId).path.has_value());
    REQUIRE(pathFollower(world, npcId).destination == simple_platformer::GridPosition{7, 1});
    REQUIRE_NEAR(pathFollower(world, npcId).repathRemaining, 0.25F);

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE_NEAR(pathFollower(world, npcId).repathRemaining, 0.15F);
    simple_platformer::updateNpcBehaviour(map, world, 0.2F);
    REQUIRE_NEAR(pathFollower(world, npcId).repathRemaining, 0.25F);
}
