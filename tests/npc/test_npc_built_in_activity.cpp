#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/actor/actor_system.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/platformer_connection_cache.hpp"
#include "simple_platformer/navigation/navigation_fill.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_system.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"
#include "support/tile_map_builder.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/add_player.hpp"
#include "support/fixed_step.hpp"
#include "support/prepare_navigation_cache.hpp"

using tests::actor;
using tests::bite;
using tests::brain;
using tests::pathFollower;
using tests::patrol;

namespace
{
    tests::ActorBuilder makePlayer(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F}).atFeet(feet).walking();
    }

    tests::ActorBuilder::Thinking makeNpc(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F})
            .atFeet(feet)
            .flying(20.0F)
            .thinking({64.0F, 1.0F});
    }
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
                           .walking()
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
                                          .walking()
                                          .thinking({64.0F, 1.0F}));
    tests::platformerMovement(world, npcId).grounded = true;
    const glm::vec2 lastKnownFeet{8.0F, 20.0F};
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastKnownTargetFeet = lastKnownFeet;
    tests::perception(world, npcId).targetVisible = false;
    tests::prepareNavigationCache(map, world);

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
    simple_platformer::updateAttacks(world, requests, 1.0F);
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
