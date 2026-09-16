#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <optional>
#include <stdexcept>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/actor/actor_system.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/npc/npc_system.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"

namespace
{
    simple_platformer::Actor makePlayer(glm::vec2 position)
    {
        simple_platformer::Actor player;
        player.body.bounds = {position, {12.0F, 12.0F}};
        player.platformerMovement = simple_platformer::PlatformerMovement{};
        player.health = simple_platformer::Health{3, 3};
        player.team = simple_platformer::Team::Player;
        return player;
    }

    simple_platformer::Actor makeNpc(glm::vec2 position)
    {
        simple_platformer::Actor npc;
        npc.body.bounds = {position, {12.0F, 12.0F}};
        npc.flyingMovement = simple_platformer::FlyingMovement{20.0F};
        npc.health = simple_platformer::Health{3, 3};
        npc.team = simple_platformer::Team::Enemy;
        npc.bite = simple_platformer::BiteAttack{};
        npc.brain = simple_platformer::NpcBrain{};
        npc.senses = simple_platformer::NpcSenses{64.0F, 1.0F};
        npc.pathFollower = simple_platformer::PathFollower{};
        return npc;
    }

    simple_platformer::Actor& actor(simple_platformer::World& world, simple_platformer::ActorId id)
    {
        simple_platformer::Actor* result = world.findActor(id);
        REQUIRE(result != nullptr);
        return *result;
    }

    simple_platformer::NpcBrain& brain(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        std::optional<simple_platformer::NpcBrain>& component = actor(world, id).brain;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no NPC brain");
        }
        return *component;
    }

    simple_platformer::BiteAttack& bite(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        std::optional<simple_platformer::BiteAttack>& component = actor(world, id).bite;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no bite");
        }
        return *component;
    }

    simple_platformer::PathFollower& pathFollower(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        std::optional<simple_platformer::PathFollower>& component = actor(world, id).pathFollower;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no path follower");
        }
        return *component;
    }

    simple_platformer::Patrol& patrol(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        std::optional<simple_platformer::Patrol>& component = actor(world, id).patrol;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no patrol");
        }
        return *component;
    }
}

TEST_CASE("NPC sight observes distance and solid tiles", "[npc][senses]")
{
    const simple_platformer::TileMap clear =
        simple_platformer::TileMap::fromAscii({".....", ".....", ".....", "#####"});
    const simple_platformer::TileMap blocked =
        simple_platformer::TileMap::fromAscii({".....", "..#..", ".....", "#####"});
    const simple_platformer::Aabb observer{{8.0F, 16.0F}, {12.0F, 12.0F}};
    const simple_platformer::Aabb target{{56.0F, 16.0F}, {12.0F, 12.0F}};

    REQUIRE(simple_platformer::canSeeTarget(clear, observer, target, {64.0F, 1.0F}));
    REQUIRE_FALSE(simple_platformer::canSeeTarget(blocked, observer, target, {64.0F, 1.0F}));
    REQUIRE_FALSE(simple_platformer::canSeeTarget(clear, observer, target, {32.0F, 1.0F}));
}

TEST_CASE("NPC target memory expires and rejects a dead player", "[npc][senses]")
{
    const simple_platformer::TileMap map = simple_platformer::TileMap::fromAscii(
        {"............", "............", "............", "############"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = world.addActor(makePlayer({32.0F, 16.0F}));
    world.setPlayer(playerId, {38.0F, 28.0F});
    const simple_platformer::ActorId npcId = world.addActor(makeNpc({16.0F, 16.0F}));

    simple_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE(brain(world, npcId).target == playerId);
    REQUIRE(brain(world, npcId).targetVisible);

    actor(world, playerId).body.bounds.position.x = 160.0F;
    simple_platformer::updateNpcSenses(map, world, 0.4F);
    REQUIRE(brain(world, npcId).target == playerId);
    REQUIRE_FALSE(brain(world, npcId).targetVisible);
    REQUIRE_THAT(
        brain(world, npcId).targetMemoryRemaining, Catch::Matchers::WithinAbs(0.6F, 0.0001F));

    simple_platformer::updateNpcSenses(map, world, 0.7F);
    REQUIRE_FALSE(brain(world, npcId).target.has_value());

    actor(world, npcId).patrol = simple_platformer::Patrol{{24.0F, 32.0F}, {72.0F, 32.0F}, true};
    brain(world, npcId).state = simple_platformer::NpcState::Chase;
    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Patrol);

    actor(world, playerId).body.bounds.position.x = 32.0F;
    simple_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE(brain(world, npcId).target == playerId);
    REQUIRE(brain(world, npcId).targetVisible);
    actor(world, playerId).life = simple_platformer::LifeState::Dying;
    simple_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE_FALSE(brain(world, npcId).target.has_value());
}

TEST_CASE("NPC systems reject invalid timing and sensing ranges", "[npc][validation]")
{
    const simple_platformer::TileMap map =
        simple_platformer::TileMap::fromAscii({"...", "...", "###"});
    const simple_platformer::Aabb bounds{{16.0F, 16.0F}, {8.0F, 8.0F}};
    simple_platformer::World world;

    REQUIRE_THROWS_AS(
        simple_platformer::canSeeTarget(map, bounds, bounds, {-1.0F, 1.0F}), std::invalid_argument);
    REQUIRE_THROWS_AS(simple_platformer::updateNpcSenses(map, world, -0.1F), std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::updateNpcBehaviour(map, world, -0.1F), std::invalid_argument);
}

TEST_CASE("A chasing NPC follows the last seen target feet", "[npc][fsm]")
{
    const simple_platformer::TileMap map =
        simple_platformer::TileMap::fromAscii({"........", "........", "########"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = world.addActor(makePlayer({64.0F, 16.0F}));
    world.setPlayer(playerId, {70.0F, 28.0F});
    const simple_platformer::ActorId npcId = world.addActor(makeNpc({18.0F, 20.0F}));
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastSeenTargetFeet = {72.0F, 32.0F};
    brain(world, npcId).targetVisible = false;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);

    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Chase);
    REQUIRE(actor(world, npcId).intentions.direction.x > 0.0F);
    REQUIRE_FALSE(actor(world, npcId).intentions.primaryAttackPressed);
}

TEST_CASE("An NPC enters bite once and returns to chase after recovery", "[npc][fsm]")
{
    const simple_platformer::TileMap map =
        simple_platformer::TileMap::fromAscii({".....", ".....", "#####"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = world.addActor(makePlayer({32.0F, 16.0F}));
    world.setPlayer(playerId, {38.0F, 28.0F});
    const simple_platformer::ActorId npcId = world.addActor(makeNpc({16.0F, 16.0F}));
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
    const simple_platformer::TileMap map =
        simple_platformer::TileMap::fromAscii({".....", ".....", "#####"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = world.addActor(makePlayer({32.0F, 16.0F}));
    world.setPlayer(playerId, {38.0F, 28.0F});
    simple_platformer::Actor npc = makeNpc({16.0F, 16.0F});
    npc.bite.reset();
    const simple_platformer::ActorId npcId = world.addActor(npc);
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastSeenTargetFeet = {38.0F, 28.0F};
    brain(world, npcId).targetVisible = true;

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);

    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Chase);
    REQUIRE_FALSE(actor(world, npcId).intentions.primaryAttackPressed);
    REQUIRE(actor(world, npcId).intentions.direction.x > 0.0F);
}

TEST_CASE("A patrol path produces intentions that move the flying NPC", "[npc][fsm]")
{
    const simple_platformer::TileMap map =
        simple_platformer::TileMap::fromAscii({"........", "........", "........", "########"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = world.addActor(makePlayer({96.0F, 32.0F}));
    world.setPlayer(playerId, {102.0F, 44.0F});
    const simple_platformer::ActorId npcId = world.addActor(makeNpc({18.0F, 20.0F}));
    actor(world, npcId).patrol = simple_platformer::Patrol{{24.0F, 32.0F}, {72.0F, 32.0F}, true};

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Patrol);
    REQUIRE(actor(world, npcId).intentions.direction.x > 0.0F);

    const float previousX = actor(world, npcId).body.bounds.position.x;
    simple_platformer::updateActorMovement(map, world, 0.1F);
    REQUIRE(actor(world, npcId).body.bounds.position.x > previousX);
}

TEST_CASE("A patrol swaps endpoints after reaching its destination", "[npc][fsm]")
{
    const simple_platformer::TileMap map =
        simple_platformer::TileMap::fromAscii({".....", ".....", "#####"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = world.addActor(makePlayer({64.0F, 0.0F}));
    world.setPlayer(playerId, {70.0F, 12.0F});
    const simple_platformer::ActorId npcId = world.addActor(makeNpc({18.0F, 20.0F}));
    actor(world, npcId).patrol = simple_platformer::Patrol{{24.0F, 32.0F}, {56.0F, 32.0F}, false};

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);

    REQUIRE(brain(world, npcId).state == simple_platformer::NpcState::Patrol);
    REQUIRE(patrol(world, npcId).headingToSecond);
    REQUIRE_FALSE(pathFollower(world, npcId).path.has_value());
}

TEST_CASE("An unreachable patrol waits before retrying its path", "[npc][fsm]")
{
    const simple_platformer::TileMap map =
        simple_platformer::TileMap::fromAscii({"....#....", "....#....", "#########"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = world.addActor(makePlayer({16.0F, 0.0F}));
    world.setPlayer(playerId, {22.0F, 12.0F});
    const simple_platformer::ActorId npcId = world.addActor(makeNpc({18.0F, 20.0F}));
    actor(world, npcId).patrol = simple_platformer::Patrol{{24.0F, 32.0F}, {120.0F, 32.0F}, true};

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE_FALSE(pathFollower(world, npcId).path.has_value());
    REQUIRE(pathFollower(world, npcId).destination == simple_platformer::GridPosition{7, 1});
    REQUIRE_THAT(
        pathFollower(world, npcId).repathRemaining, Catch::Matchers::WithinAbs(0.25F, 0.0001F));

    simple_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE_THAT(
        pathFollower(world, npcId).repathRemaining, Catch::Matchers::WithinAbs(0.15F, 0.0001F));
    simple_platformer::updateNpcBehaviour(map, world, 0.2F);
    REQUIRE_THAT(
        pathFollower(world, npcId).repathRemaining, Catch::Matchers::WithinAbs(0.25F, 0.0001F));
}
