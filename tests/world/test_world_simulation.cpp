#include <catch2/catch_message.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
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
    player.intentions.aimDirection = {1.0F, 0.0F};
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

TEST_CASE("World simulation lets a ranged NPC shoot a visible player", "[world][simulation]")
{
    const simple_platformer::TileMap map =
        simple_platformer::TileMap::fromAscii({".....", ".....", "#####"});
    simple_platformer::World world;

    simple_platformer::Actor player;
    player.body.bounds = {{48.0F, 16.0F}, {12.0F, 12.0F}};
    player.platformerMovement = simple_platformer::PlatformerMovement{};
    player.health = simple_platformer::Health{3, 3};
    player.team = simple_platformer::Team::Player;
    const simple_platformer::ActorId playerId = world.addActor(player);
    world.setPlayer(playerId, {54.0F, 28.0F});

    simple_platformer::Actor npc;
    npc.body.bounds = {{16.0F, 16.0F}, {12.0F, 12.0F}};
    npc.flyingMovement = simple_platformer::FlyingMovement{60.0F};
    npc.health = simple_platformer::Health{3, 3};
    npc.team = simple_platformer::Team::Enemy;
    npc.rangedWeapon = simple_platformer::RangedWeapon{};
    npc.brain = simple_platformer::NpcBrain{};
    npc.senses = simple_platformer::NpcSenses{96.0F, 1.0F};
    npc.pathFollower = simple_platformer::PathFollower{};
    const simple_platformer::ActorId npcId = world.addActor(npc);

    simple_platformer::updateWorldSimulation(map, world, 1.0F / 60.0F);

    REQUIRE(world.projectiles().size() == 1);
    REQUIRE(world.projectiles().front().owner == npcId);
    REQUIRE(world.projectiles().front().velocity.x > 0.0F);
    const simple_platformer::Actor* storedNpc = world.findActor(npcId);
    REQUIRE(storedNpc != nullptr);
    REQUIRE(storedNpc->body.bounds.position.x == 16.0F);
}

TEST_CASE("World simulation continuously patrols a ground NPC", "[world][simulation]")
{
    const simple_platformer::TileMap map = simple_platformer::TileMap::fromAscii(
        {"..........", "..........", "..........", "....###...", "..........", "##########"});
    simple_platformer::World world;
    constexpr simple_platformer::GridPosition LowerEndpoint{2, 4};
    constexpr simple_platformer::GridPosition UpperEndpoint{4, 2};
    const glm::vec2 lowerFeet = simple_platformer::navigationFeet(LowerEndpoint);
    const glm::vec2 upperFeet = simple_platformer::navigationFeet(UpperEndpoint);

    simple_platformer::Actor npc;
    npc.body.bounds = {{0.0F, 0.0F}, {12.0F, 12.0F}};
    simple_platformer::placeFeetAt(npc.body.bounds, lowerFeet);
    npc.platformerMovement = simple_platformer::PlatformerMovement{};
    npc.platformerMovement->grounded = true;
    npc.brain = simple_platformer::NpcBrain{};
    npc.senses = simple_platformer::NpcSenses{};
    npc.patrol = simple_platformer::Patrol{lowerFeet, upperFeet, true};
    npc.pathFollower = simple_platformer::PathFollower{};
    const simple_platformer::ActorId npcId = world.addActor(npc);

    bool enteredPatrol = false;
    bool wasAirborne = false;
    bool reachedUpperEndpoint = false;
    bool switchedTowardLowerEndpoint = false;
    bool returnedToLowerEndpoint = false;
    int completedPatrolLegs = 0;
    bool previousHeadingToSecond = true;
    for (int tick = 0; tick < 1200 && completedPatrolLegs < 4; ++tick)
    {
        simple_platformer::updateWorldSimulation(map, world, 1.0F / 60.0F);
        const simple_platformer::Actor* storedNpc = world.findActor(npcId);
        REQUIRE(storedNpc != nullptr);
        if (!storedNpc->platformerMovement.has_value() || !storedNpc->brain.has_value() ||
            !storedNpc->patrol.has_value())
        {
            throw std::logic_error("The test NPC is missing its patrol components");
        }
        const simple_platformer::PlatformerMovement& movement =
            storedNpc->platformerMovement.value();
        const simple_platformer::Patrol& patrol = storedNpc->patrol.value();
        const simple_platformer::GridPosition cell =
            simple_platformer::navigationCell(simple_platformer::feetOf(storedNpc->body.bounds));

        enteredPatrol =
            enteredPatrol || storedNpc->brain->state == simple_platformer::NpcState::Patrol;
        wasAirborne = wasAirborne || !movement.grounded;
        reachedUpperEndpoint = reachedUpperEndpoint || (movement.grounded && cell == UpperEndpoint);
        switchedTowardLowerEndpoint =
            switchedTowardLowerEndpoint || (reachedUpperEndpoint && !patrol.headingToSecond);
        returnedToLowerEndpoint =
            returnedToLowerEndpoint ||
            (switchedTowardLowerEndpoint && movement.grounded && cell == LowerEndpoint);
        if (patrol.headingToSecond != previousHeadingToSecond)
        {
            ++completedPatrolLegs;
            previousHeadingToSecond = patrol.headingToSecond;
        }
    }

    REQUIRE(enteredPatrol);
    REQUIRE(wasAirborne);
    REQUIRE(reachedUpperEndpoint);
    REQUIRE(switchedTowardLowerEndpoint);
    REQUIRE(returnedToLowerEndpoint);
    REQUIRE(completedPatrolLegs == 4);
}

TEST_CASE(
    "A bat continuously patrols around a platform corner",
    "[world][simulation][flying][regression]")
{
    const simple_platformer::TileMap map = simple_platformer::TileMap::fromAscii(
        {"..........", "..........", "..........", "....###...", "..........", "##########"});
    // The bat must rise beside the platform before turning over its top edge.
    // Both endpoints are reachable with ample clearance for its 12 x 8 body.
    const glm::vec2 lowerFeet = GENERATE(glm::vec2{56.0F, 80.0F}, glm::vec2{120.0F, 80.0F});
    const glm::vec2 upperFeet{88.0F, 48.0F};

    simple_platformer::Actor bat;
    bat.body.bounds.size = {12.0F, 8.0F};
    simple_platformer::placeFeetAt(bat.body.bounds, lowerFeet);
    bat.flyingMovement = simple_platformer::FlyingMovement{};
    bat.brain = simple_platformer::NpcBrain{};
    bat.senses = simple_platformer::NpcSenses{};
    bat.patrol = simple_platformer::Patrol{lowerFeet, upperFeet, true};
    bat.pathFollower = simple_platformer::PathFollower{};
    simple_platformer::World world;
    const simple_platformer::ActorId batId = world.addActor(bat);

    int completedPatrolLegs = 0;
    bool headingToSecond = true;
    for (int tick = 0; tick < 1200 && completedPatrolLegs < 4; ++tick)
    {
        simple_platformer::updateWorldSimulation(map, world, 1.0F / 60.0F);
        const simple_platformer::Actor* storedBat = world.findActor(batId);
        REQUIRE(storedBat != nullptr);
        if (!storedBat->patrol.has_value())
        {
            throw std::logic_error("The test bat has no patrol");
        }
        if (storedBat->patrol->headingToSecond != headingToSecond)
        {
            const glm::vec2 expectedFeet = headingToSecond ? upperFeet : lowerFeet;
            const glm::vec2 actualFeet = simple_platformer::feetOf(storedBat->body.bounds);
            CAPTURE(tick, completedPatrolLegs, actualFeet.x, actualFeet.y);
            REQUIRE(glm::distance(actualFeet, expectedFeet) <= 2.0F);
            ++completedPatrolLegs;
            headingToSecond = storedBat->patrol->headingToSecond;
        }
    }

    const simple_platformer::Actor* storedBat = world.findActor(batId);
    REQUIRE(storedBat != nullptr);
    if (!storedBat->pathFollower.has_value())
    {
        throw std::logic_error("The test bat has no path follower");
    }
    const glm::vec2 finalFeet = simple_platformer::feetOf(storedBat->body.bounds);
    CAPTURE(finalFeet.x, finalFeet.y, storedBat->pathFollower->nextStep, completedPatrolLegs);
    REQUIRE(completedPatrolLegs == 4);
}
