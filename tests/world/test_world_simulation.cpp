#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_simulation.hpp"

TEST_CASE("World simulation spawns a projectile after projectile movement", "[world][simulation]")
{
    const simple_platformer::TileMap map =
        simple_platformer::TileMap::fromAscii({".....", ".....", "#####"});
    simple_platformer::World world;
    simple_platformer::Actor player;
    player.body.bounds = {{16.0F, 16.0F}, {12.0F, 12.0F}};
    player.platformerMovement = simple_platformer::PlatformerMovement{};
    player.team = simple_platformer::Team::Player;
    player.rangedWeapon = simple_platformer::RangedWeapon{};
    player.intentions.primaryAttackPressed = true;
    const simple_platformer::ActorId playerId = world.addActor(player);

    simple_platformer::updateWorldSimulation(map, world, 1.0F / 60.0F);

    REQUIRE(world.projectiles().size() == 1);
    const float spawnPosition = world.projectiles().front().bounds.position.x;

    simple_platformer::Actor* storedPlayer = world.findActor(playerId);
    REQUIRE(storedPlayer != nullptr);
    storedPlayer->intentions.primaryAttackPressed = false;
    simple_platformer::updateWorldSimulation(map, world, 1.0F / 60.0F);

    REQUIRE(world.projectiles().size() == 1);
    REQUIRE(world.projectiles().front().bounds.position.x > spawnPosition);
}

TEST_CASE("World simulation senses decides and moves an NPC in one update", "[world][simulation]")
{
    const simple_platformer::TileMap map =
        simple_platformer::TileMap::fromAscii({"........", "........", "########"});
    simple_platformer::World world;

    simple_platformer::Actor player;
    player.body.bounds = {{64.0F, 16.0F}, {12.0F, 12.0F}};
    player.platformerMovement = simple_platformer::PlatformerMovement{};
    player.health = simple_platformer::Health{3, 3};
    player.team = simple_platformer::Team::Player;
    const simple_platformer::ActorId playerId = world.addActor(player);
    world.setPlayer(playerId, {70.0F, 28.0F});

    simple_platformer::Actor npc;
    npc.body.bounds = {{16.0F, 16.0F}, {12.0F, 12.0F}};
    npc.flyingMovement = simple_platformer::FlyingMovement{60.0F};
    npc.health = simple_platformer::Health{3, 3};
    npc.team = simple_platformer::Team::Enemy;
    npc.bite = simple_platformer::BiteAttack{};
    npc.brain = simple_platformer::NpcBrain{};
    npc.senses = simple_platformer::NpcSenses{96.0F, 1.0F};
    npc.pathFollower = simple_platformer::PathFollower{};
    const simple_platformer::ActorId npcId = world.addActor(npc);

    simple_platformer::updateWorldSimulation(map, world, 1.0F / 60.0F);

    const simple_platformer::Actor* storedNpc = world.findActor(npcId);
    REQUIRE(storedNpc != nullptr);
    if (!storedNpc->brain.has_value())
    {
        throw std::logic_error("The test NPC has no brain");
    }
    const simple_platformer::NpcBrain& brain = *storedNpc->brain;
    REQUIRE(brain.target == playerId);
    REQUIRE(brain.state == simple_platformer::NpcState::Chase);
    REQUIRE(storedNpc->body.bounds.position.x > 16.0F);
}
