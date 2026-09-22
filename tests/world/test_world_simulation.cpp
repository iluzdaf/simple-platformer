#include <catch2/catch_message.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/catch_test_macros.hpp>

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
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/timing/fixed_step.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_simulation.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"

TEST_CASE("World simulation advances its shared clock once per update", "[world][simulation][time]")
{
    simple_platformer::TileMap map = tests::TileMapBuilder({"."});
    simple_platformer::World world;

    simple_platformer::updateWorldSimulation(map, world, 0.25F);
    simple_platformer::updateWorldSimulation(map, world, 0.25F);

    REQUIRE(world.simulationTimeSeconds() == 0.5F);

    world.completeLevel();
    simple_platformer::updateWorldSimulation(map, world, 0.25F);

    REQUIRE(world.simulationTimeSeconds() == 0.5F);
}

TEST_CASE("World simulation spawns a projectile after projectile movement", "[world][simulation]")
{
    simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::World world;
    simple_platformer::Actor player = tests::ActorBuilder::sized({12.0F, 12.0F})
                                          .atFeet({22.0F, 28.0F})
                                          .walking()
                                          .onTeam(simple_platformer::Team::Player)
                                          .shooting();
    player.intentions.aimDirection = {1.0F, 0.0F};
    player.intentions.primaryAttackPressed = true;
    const simple_platformer::ActorId playerId = world.addActor(player);

    simple_platformer::updateWorldSimulation(map, world, 1.0F / 60.0F);

    REQUIRE(world.projectiles().size() == 1);
    const float spawnPosition = world.projectiles().front().bounds.position.x;

    simple_platformer::Actor& storedPlayer = tests::actor(world, playerId);
    storedPlayer.intentions.primaryAttackPressed = false;
    simple_platformer::updateWorldSimulation(map, world, 1.0F / 60.0F);

    REQUIRE(world.projectiles().size() == 1);
    REQUIRE(world.projectiles().front().bounds.position.x > spawnPosition);
}

TEST_CASE("World simulation senses decides and moves an NPC in one update", "[world][simulation]")
{
    simple_platformer::TileMap map = tests::TileMapBuilder({"........", "........", "########"});
    simple_platformer::World world;

    simple_platformer::Actor player = tests::ActorBuilder::sized({12.0F, 12.0F})
                                          .atFeet({70.0F, 28.0F})
                                          .walking()
                                          .withHealth(3, 3)
                                          .onTeam(simple_platformer::Team::Player);
    const simple_platformer::ActorId playerId = world.addActor(player);
    world.setPlayer(playerId, {70.0F, 28.0F});

    simple_platformer::Actor npc = tests::ActorBuilder::sized({12.0F, 12.0F})
                                       .atFeet({22.0F, 28.0F})
                                       .flying(60.0F)
                                       .withHealth(3, 3)
                                       .onTeam(simple_platformer::Team::Enemy)
                                       .biting()
                                       .thinking({96.0F, 1.0F});
    const simple_platformer::ActorId npcId = world.addActor(npc);

    simple_platformer::updateWorldSimulation(map, world, 1.0F / 60.0F);

    simple_platformer::Actor& storedNpc = tests::actor(world, npcId);
    const simple_platformer::NpcBrain& brain = tests::brain(storedNpc);
    REQUIRE(brain.target == playerId);
    REQUIRE(brain.state == simple_platformer::NpcState::Chase);
    REQUIRE(storedNpc.body.bounds.position.x > 16.0F);
}

TEST_CASE("World simulation lets a ranged NPC shoot a visible player", "[world][simulation]")
{
    simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    simple_platformer::World world;

    simple_platformer::Actor player = tests::ActorBuilder::sized({12.0F, 12.0F})
                                          .atFeet({54.0F, 28.0F})
                                          .walking()
                                          .withHealth(3, 3)
                                          .onTeam(simple_platformer::Team::Player);
    const simple_platformer::ActorId playerId = world.addActor(player);
    world.setPlayer(playerId, {54.0F, 28.0F});

    simple_platformer::Actor npc = tests::ActorBuilder::sized({12.0F, 12.0F})
                                       .atFeet({22.0F, 28.0F})
                                       .flying(60.0F)
                                       .withHealth(3, 3)
                                       .onTeam(simple_platformer::Team::Enemy)
                                       .shooting()
                                       .thinking({96.0F, 1.0F});
    const simple_platformer::ActorId npcId = world.addActor(npc);

    simple_platformer::updateWorldSimulation(map, world, 1.0F / 60.0F);

    REQUIRE(world.projectiles().size() == 1);
    REQUIRE(world.projectiles().front().owner == npcId);
    REQUIRE(world.projectiles().front().velocity.x > 0.0F);
    simple_platformer::Actor& storedNpc = tests::actor(world, npcId);
    REQUIRE(storedNpc.body.bounds.position.x == 16.0F);
}

TEST_CASE("World simulation lets an NPC hear a shot on the next update", "[world][simulation]")
{
    simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "...x....", "########"})
            .where('x', tests::Tile().blocksSight());
    simple_platformer::World world;

    simple_platformer::Actor player = tests::ActorBuilder::sized({12.0F, 12.0F})
                                          .atFeet({86.0F, 48.0F})
                                          .walking()
                                          .withHealth(3, 3)
                                          .onTeam(simple_platformer::Team::Player)
                                          .shooting();
    player.intentions.aimDirection = {1.0F, 0.0F};
    player.intentions.primaryAttackPressed = true;
    const simple_platformer::ActorId playerId = world.addActor(player);
    world.setPlayer(playerId, {86.0F, 48.0F});

    simple_platformer::Actor npc = tests::ActorBuilder::sized({12.0F, 12.0F})
                                       .atFeet({22.0F, 48.0F})
                                       .flying(60.0F)
                                       .withHealth(3, 3)
                                       .onTeam(simple_platformer::Team::Enemy)
                                       .thinking({96.0F, 1.0F});
    const simple_platformer::ActorId npcId = world.addActor(npc);

    // Senses run before attacks, so the update that fires is not yet heard.
    simple_platformer::updateWorldSimulation(map, world, 1.0F / 60.0F);
    REQUIRE(world.projectiles().size() == 1);
    simple_platformer::Actor& storedNpc = tests::actor(world, npcId);
    REQUIRE_FALSE(tests::brain(storedNpc).target.has_value());

    simple_platformer::Actor& storedPlayer = tests::actor(world, playerId);
    storedPlayer.intentions.primaryAttackPressed = false;
    simple_platformer::updateWorldSimulation(map, world, 1.0F / 60.0F);

    REQUIRE(tests::brain(world, npcId).target == playerId);
    REQUIRE_FALSE(tests::brain(world, npcId).targetVisible);
    REQUIRE(tests::brain(world, npcId).state == simple_platformer::NpcState::Chase);
}

TEST_CASE("World simulation continuously patrols a ground NPC", "[world][simulation]")
{
    simple_platformer::TileMap map = tests::TileMapBuilder(
        {"..........", "..........", "..........", "....###...", "..........", "##########"});
    simple_platformer::World world;
    constexpr simple_platformer::GridPosition LowerEndpoint{2, 4};
    constexpr simple_platformer::GridPosition UpperEndpoint{4, 2};
    const glm::vec2 lowerFeet = simple_platformer::feetInCell(tests::TileSize, LowerEndpoint);
    const glm::vec2 upperFeet = simple_platformer::feetInCell(tests::TileSize, UpperEndpoint);

    simple_platformer::Actor npc = tests::ActorBuilder::sized({12.0F, 12.0F})
                                       .atFeet(lowerFeet)
                                       .walking()
                                       .patrolling(lowerFeet, upperFeet)
                                       .thinking({});
    tests::platformerMovement(npc).grounded = true;
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
        simple_platformer::Actor& storedNpc = tests::actor(world, npcId);
        const simple_platformer::PlatformerMovement& movement =
            tests::platformerMovement(storedNpc);
        const simple_platformer::Patrol& patrol = tests::patrol(storedNpc);
        const simple_platformer::GridPosition cell = simple_platformer::cellAtFeet(
            tests::TileSize, simple_platformer::feetOf(storedNpc.body.bounds));

        enteredPatrol =
            enteredPatrol || tests::brain(storedNpc).state == simple_platformer::NpcState::Patrol;
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
    "A slow ground NPC stays grounded on a long lower-platform patrol",
    "[world][simulation][platformer][regression]")
{
    // The raised platform provides an unnecessary jump route above the continuous floor.
    simple_platformer::TileMap map =
        tests::TileMapBuilder({".....###.....", ".............", ".............", "#############"});
    simple_platformer::World world;
    constexpr simple_platformer::GridPosition FirstEndpoint{4, 2};
    constexpr simple_platformer::GridPosition SpawnCell{12, 2};
    constexpr simple_platformer::GridPosition SecondEndpoint{12, 2};
    const glm::vec2 firstFeet = simple_platformer::feetInCell(tests::TileSize, FirstEndpoint);
    const glm::vec2 secondFeet = simple_platformer::feetInCell(tests::TileSize, SecondEndpoint);

    simple_platformer::Actor npc = tests::ActorBuilder::sized({12.0F, 20.0F})
                                       .inCell(SpawnCell)
                                       .walking()
                                       .patrolling(firstFeet, secondFeet)
                                       .thinking({});
    tests::platformerMovement(npc).config.maximumSpeed = 60.0F;
    tests::platformerMovement(npc).grounded = true;
    tests::patrol(npc).headingToSecond = false;
    const simple_platformer::ActorId npcId = world.addActor(npc);

    bool becameAirborne = false;
    bool completedPatrolLeg = false;
    for (int tick = 0; tick < 900 && !completedPatrolLeg; ++tick)
    {
        simple_platformer::updateWorldSimulation(map, world, 1.0F / 60.0F);
        simple_platformer::Actor& storedNpc = tests::actor(world, npcId);
        becameAirborne = becameAirborne || !tests::platformerMovement(storedNpc).grounded;
        completedPatrolLeg = tests::patrol(storedNpc).headingToSecond;
    }

    REQUIRE(completedPatrolLeg);
    REQUIRE_FALSE(becameAirborne);
}

TEST_CASE(
    "A ground NPC resumes patrol after forgetting its target at a platform edge",
    "[world][simulation][platformer][regression]")
{
    simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "..###...", "........", "########"});
    simple_platformer::World world;

    simple_platformer::Actor player = tests::ActorBuilder::sized({12.0F, 12.0F})
                                          .atFeet({104.0F, 64.0F})
                                          .walking()
                                          .withHealth(3, 3)
                                          .onTeam(simple_platformer::Team::Player);
    const simple_platformer::ActorId playerId = world.addActor(player);
    world.setPlayer(playerId, simple_platformer::feetOf(player.body.bounds));

    constexpr simple_platformer::GridPosition LeftPatrolCell{2, 1};
    constexpr simple_platformer::GridPosition RightPatrolCell{4, 1};
    const glm::vec2 leftPatrolFeet = simple_platformer::feetInCell(tests::TileSize, LeftPatrolCell);
    const glm::vec2 rightPatrolFeet =
        simple_platformer::feetInCell(tests::TileSize, RightPatrolCell);

    constexpr float PlatformRightEdge = 80.0F;
    // Its feet have crossed into the unsupported cell, but the left side of its
    // collider still overlaps the platform and remains grounded.
    simple_platformer::Actor zombie = tests::ActorBuilder::sized({12.0F, 20.0F})
                                          .atFeet({PlatformRightEdge + 0.5F, 32.0F})
                                          .walking()
                                          .withHealth(3, 3)
                                          .onTeam(simple_platformer::Team::Enemy)
                                          .patrolling(leftPatrolFeet, rightPatrolFeet)
                                          .thinking({16.0F, 0.01F});
    // Preserve the movement that carried it toward the last-seen player position.
    zombie.body.velocity.x = 100.0F;
    tests::platformerMovement(zombie).grounded = true;
    tests::brain(zombie).state = simple_platformer::NpcState::Chase;
    tests::brain(zombie).target = playerId;
    tests::brain(zombie).lastSeenTargetFeet = {88.0F, 32.0F};
    tests::brain(zombie).targetMemoryRemaining = 0.01F;
    tests::patrol(zombie).headingToSecond = false;
    const simple_platformer::ActorId zombieId = world.addActor(zombie);

    simple_platformer::updateWorldSimulation(map, world, 1.0F / 60.0F);

    simple_platformer::Actor& storedZombie = tests::actor(world, zombieId);
    REQUIRE(tests::brain(storedZombie).state == simple_platformer::NpcState::Patrol);
    REQUIRE_FALSE(tests::brain(storedZombie).target.has_value());

    for (int tick = 0; tick < 180; ++tick)
    {
        simple_platformer::updateWorldSimulation(map, world, 1.0F / 60.0F);
    }

    const glm::vec2 finalFeet =
        simple_platformer::feetOf(tests::actor(world, zombieId).body.bounds);
    CAPTURE(finalFeet.x, finalFeet.y);
    REQUIRE(finalFeet.x < rightPatrolFeet.x);
}

TEST_CASE(
    "A ground NPC pursues a remembered airborne position after losing sight of the player",
    "[world][simulation][platformer][regression]")
{
    // The player jumps from the floor beside the raised platform. Its solid
    // edge hides the player before the jump and again after landing.
    simple_platformer::TileMap map = tests::TileMapBuilder(
        {"..........", "..........", "...#######", "..........", "##########"});
    constexpr float DeltaTime = static_cast<float>(simple_platformer::FixedDeltaSeconds);
    constexpr int JumpAndLandingTicks = 40;
    constexpr int RememberedChaseTicks = 30;

    simple_platformer::World world;
    simple_platformer::Actor player = tests::ActorBuilder::sized({12.0F, 20.0F})
                                          .inCell({2, 3})
                                          .walking()
                                          .onTeam(simple_platformer::Team::Player);
    tests::platformerMovement(player).grounded = true;
    const simple_platformer::ActorId playerId = world.addActor(player);
    world.setPlayer(playerId, simple_platformer::feetOf(player.body.bounds));

    simple_platformer::Actor zombie = tests::ActorBuilder::sized({12.0F, 20.0F})
                                          .inCell({7, 1})
                                          .walking()
                                          .onTeam(simple_platformer::Team::Enemy)
                                          .thinking({});
    tests::platformerMovement(zombie).grounded = true;
    const simple_platformer::ActorId zombieId = world.addActor(zombie);

    // Jump into view, then land behind the upper platform's solid edge.
    // Sensing and behaviour, rather than the test, set the zombie's memory/state.
    bool seenDuringJump = false;
    for (int tick = 0; tick < JumpAndLandingTicks; ++tick)
    {
        simple_platformer::Actor& storedPlayer = tests::actor(world, playerId);
        storedPlayer.intentions.jumpPressed = tick == 0;
        storedPlayer.intentions.jumpHeld = tick < 25;
        simple_platformer::updateWorldSimulation(map, world, DeltaTime);
        simple_platformer::Actor& storedZombie = tests::actor(world, zombieId);
        seenDuringJump = seenDuringJump ||
                         (tests::brain(storedZombie).targetVisible &&
                          tests::brain(storedZombie).state == simple_platformer::NpcState::Chase);
    }
    REQUIRE(seenDuringJump);
    simple_platformer::Actor& rememberedZombie = tests::actor(world, zombieId);
    REQUIRE_FALSE(tests::brain(rememberedZombie).targetVisible);
    REQUIRE(tests::brain(rememberedZombie).target == playerId);
    REQUIRE(tests::brain(rememberedZombie).targetMemoryRemaining > 0.0F);
    REQUIRE(tests::brain(rememberedZombie).state == simple_platformer::NpcState::Chase);
    const glm::vec2 lastSeenFeet = tests::brain(rememberedZombie).lastSeenTargetFeet;
    REQUIRE_FALSE(simple_platformer::canStandAt(
        map,
        simple_platformer::cellAtFeet(tests::TileSize, lastSeenFeet),
        rememberedZombie.body.bounds.size));
    const float startingDistance =
        glm::distance(simple_platformer::feetOf(rememberedZombie.body.bounds), lastSeenFeet);

    // The remembered point is in the air, but the zombie can approach the
    // platform edge toward it. Check progress without prescribing a goal cell.
    float distanceToRememberedPosition = startingDistance;
    for (int tick = 0; tick < RememberedChaseTicks; ++tick)
    {
        simple_platformer::updateWorldSimulation(map, world, DeltaTime);
        simple_platformer::Actor& storedZombie = tests::actor(world, zombieId);
        REQUIRE_FALSE(tests::brain(storedZombie).targetVisible);
        REQUIRE(tests::brain(storedZombie).target == playerId);
        REQUIRE(tests::brain(storedZombie).targetMemoryRemaining > 0.0F);
        REQUIRE(tests::brain(storedZombie).lastSeenTargetFeet == lastSeenFeet);
        REQUIRE(tests::brain(storedZombie).state == simple_platformer::NpcState::Chase);
        distanceToRememberedPosition =
            glm::distance(simple_platformer::feetOf(storedZombie.body.bounds), lastSeenFeet);
    }
    CAPTURE(startingDistance, distanceToRememberedPosition);
    constexpr float MinimumPursuitProgress = 0.5F * tests::TileSize;
    REQUIRE(distanceToRememberedPosition < startingDistance - MinimumPursuitProgress);
}

TEST_CASE(
    "A ground NPC approaches a visible player supported at a platform edge",
    "[world][simulation][platformer][regression]")
{
    simple_platformer::TileMap map = tests::TileMapBuilder(
        {"..........", "..........", "..#######.", "..........", "##########"});
    constexpr float DeltaTime = static_cast<float>(simple_platformer::FixedDeltaSeconds);
    constexpr int MaximumChaseTicks = 180;
    const glm::vec2 upperPlatformFeet = simple_platformer::feetInCell(tests::TileSize, {2, 1});
    const float platformLeftEdge = simple_platformer::gridToWorld(tests::TileSize, {2, 2}).x;
    // Control: feet at the first cell's centre. Regression: feet just outside
    // the platform, while part of the player's collider is still supported.
    const bool feetOutsidePlatform = GENERATE(false, true);
    const glm::vec2 playerFeet{
        feetOutsidePlatform ? platformLeftEdge - 0.5F : upperPlatformFeet.x, upperPlatformFeet.y};
    CAPTURE(feetOutsidePlatform);

    simple_platformer::World world;
    simple_platformer::Actor player = tests::ActorBuilder::sized({12.0F, 20.0F})
                                          .atFeet(playerFeet)
                                          .walking()
                                          .onTeam(simple_platformer::Team::Player);
    tests::platformerMovement(player).grounded = true;
    const simple_platformer::ActorId playerId = world.addActor(player);
    world.setPlayer(playerId, playerFeet);

    simple_platformer::Actor zombie = tests::ActorBuilder::sized({12.0F, 20.0F})
                                          .inCell({6, 1})
                                          .walking()
                                          .onTeam(simple_platformer::Team::Enemy)
                                          .biting()
                                          .thinking({});
    tests::platformerMovement(zombie).grounded = true;
    const simple_platformer::ActorId zombieId = world.addActor(zombie);

    simple_platformer::updateWorldSimulation(map, world, DeltaTime);

    simple_platformer::Actor& chasingZombie = tests::actor(world, zombieId);
    REQUIRE(tests::brain(chasingZombie).targetVisible);
    REQUIRE(tests::brain(chasingZombie).state == simple_platformer::NpcState::Chase);
    const float startingDistance =
        glm::distance(simple_platformer::feetOf(chasingZombie.body.bounds), playerFeet);
    constexpr float CloseDistance = 2.0F * tests::TileSize;
    REQUIRE(startingDistance > CloseDistance);

    float distanceToPlayer = startingDistance;
    for (int tick = 0; tick < MaximumChaseTicks && distanceToPlayer > CloseDistance; ++tick)
    {
        simple_platformer::updateWorldSimulation(map, world, DeltaTime);
        simple_platformer::Actor& storedZombie = tests::actor(world, zombieId);
        simple_platformer::Actor& storedPlayer = tests::actor(world, playerId);
        REQUIRE(tests::brain(storedZombie).targetVisible);
        REQUIRE(tests::platformerMovement(storedPlayer).grounded);
        REQUIRE(simple_platformer::feetOf(storedPlayer.body.bounds) == playerFeet);
        distanceToPlayer =
            glm::distance(simple_platformer::feetOf(storedZombie.body.bounds), playerFeet);
    }
    CAPTURE(startingDistance, distanceToPlayer);
    REQUIRE(distanceToPlayer <= CloseDistance);
}

TEST_CASE(
    "A bat continuously patrols around a platform corner",
    "[world][simulation][flying][regression]")
{
    simple_platformer::TileMap map = tests::TileMapBuilder(
        {"..........", "..........", "..........", "....###...", "..........", "##########"});
    // The bat must rise beside the platform before turning over its top edge.
    // Both endpoints are reachable with ample clearance for its 12 x 8 body.
    const glm::vec2 lowerFeet = GENERATE(glm::vec2{56.0F, 80.0F}, glm::vec2{120.0F, 80.0F});
    const glm::vec2 upperFeet{88.0F, 48.0F};

    simple_platformer::Actor bat = tests::ActorBuilder::sized({12.0F, 8.0F})
                                       .atFeet(lowerFeet)
                                       .flying(60.0F)
                                       .patrolling(lowerFeet, upperFeet)
                                       .thinking({});
    simple_platformer::World world;
    const simple_platformer::ActorId batId = world.addActor(bat);

    int completedPatrolLegs = 0;
    bool headingToSecond = true;
    for (int tick = 0; tick < 1200 && completedPatrolLegs < 4; ++tick)
    {
        simple_platformer::updateWorldSimulation(map, world, 1.0F / 60.0F);
        simple_platformer::Actor& storedBat = tests::actor(world, batId);
        if (tests::patrol(storedBat).headingToSecond != headingToSecond)
        {
            const glm::vec2 expectedFeet = headingToSecond ? upperFeet : lowerFeet;
            const glm::vec2 actualFeet = simple_platformer::feetOf(storedBat.body.bounds);
            CAPTURE(tick, completedPatrolLegs, actualFeet.x, actualFeet.y);
            REQUIRE(glm::distance(actualFeet, expectedFeet) <= 2.0F);
            ++completedPatrolLegs;
            headingToSecond = tests::patrol(storedBat).headingToSecond;
        }
    }

    simple_platformer::Actor& storedBat = tests::actor(world, batId);
    const glm::vec2 finalFeet = simple_platformer::feetOf(storedBat.body.bounds);
    CAPTURE(finalFeet.x, finalFeet.y, tests::pathFollower(storedBat).nextStep, completedPatrolLegs);
    REQUIRE(completedPatrolLegs == 4);
}
