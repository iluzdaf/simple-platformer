#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "debug/debug_overlay.hpp"
#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/tile_map_builder.hpp"

TEST_CASE("Debug overlay data supports actors without presentation components", "[app][debug]")
{
    simple_platformer::Actor actor;
    actor.body.bounds = {{12.0F, 20.0F}, {8.0F, 10.0F}};
    actor.platformerMovement = simple_platformer::PlatformerMovement{};

    simple_platformer::World world;
    const simple_platformer::ActorId id = world.addActor(actor);
    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", "####"});
    const simple_platformer::Camera camera{{4.0F, 5.0F}, {320.0F, 180.0F}};
    const simple_platformer::CameraController cameraController{camera, {80.0F, 40.0F}};

    const simple_platformer::DebugOverlay debug =
        simple_platformer::makeDebugOverlay(world, map, cameraController, 128.0F);

    REQUIRE(debug.cameraBounds.position == camera.position);
    REQUIRE(debug.cameraBounds.size == camera.viewportSize);
    REQUIRE(debug.cameraDeadZone.position == glm::vec2{124.0F, 75.0F});
    REQUIRE(debug.cameraDeadZone.size == cameraController.deadZoneSize);
    REQUIRE(debug.actors.size() == 1);
    REQUIRE(debug.actors.front().id == id);
    REQUIRE(debug.actors.front().kind == simple_platformer::ActorDebugKind::Actor);
    REQUIRE(debug.actors.front().collider.position == actor.body.bounds.position);
    REQUIRE(debug.actors.front().collider.size == actor.body.bounds.size);
    REQUIRE_FALSE(debug.actors.front().sprite.has_value());
    REQUIRE_FALSE(debug.actors.front().animation.has_value());
    REQUIRE_FALSE(debug.actors.front().npcState.has_value());
    REQUIRE_FALSE(debug.actors.front().pathFollower.has_value());
    REQUIRE_FALSE(debug.actors.front().sensor.has_value());
    REQUIRE_FALSE(debug.actors.front().patrol.has_value());
    REQUIRE_FALSE(debug.actors.front().biteHitbox.has_value());
}

TEST_CASE("Debug overlay data describes NPC patrol points", "[app][debug]")
{
    simple_platformer::Actor npc;
    npc.body.bounds = {{16.0F, 20.0F}, {12.0F, 12.0F}};
    npc.platformerMovement = simple_platformer::PlatformerMovement{};
    npc.brain = simple_platformer::NpcBrain{};
    npc.senses = simple_platformer::NpcSenses{};
    npc.pathFollower = simple_platformer::PathFollower{};
    npc.patrol = simple_platformer::Patrol{{24.0F, 32.0F}, {72.0F, 32.0F}, false};

    simple_platformer::World world;
    world.addActor(npc);
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", "#####"});
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};

    const simple_platformer::DebugOverlay debug =
        simple_platformer::makeDebugOverlay(world, map, cameraController, 128.0F);

    REQUIRE(debug.actors.front().patrol.has_value());
    const simple_platformer::PatrolDebugInfo patrol =
        debug.actors.front().patrol.value_or(simple_platformer::PatrolDebugInfo{});
    REQUIRE(patrol.firstFeet == glm::vec2{24.0F, 32.0F});
    REQUIRE(patrol.secondFeet == glm::vec2{72.0F, 32.0F});
    REQUIRE_FALSE(patrol.headingToSecond);
}

TEST_CASE("Debug overlay data reports player presentation and NPC state", "[app][debug]")
{
    const simple_platformer::SpriteRegion region{{64.0F, 24.0F}, {32.0F, 24.0F}};
    simple_platformer::Animator animator;
    animator.current = simple_platformer::AnimationName::Move;
    animator.animationSet.clips.push_back(
        {simple_platformer::AnimationName::Move, {region}, 0.1F, true});

    simple_platformer::Actor player;
    player.body.bounds = {{32.0F, 196.0F}, {12.0F, 12.0F}};
    player.platformerMovement = simple_platformer::PlatformerMovement{};
    player.sprite = simple_platformer::Sprite{1, region, {32.0F, 24.0F}};
    player.animator = animator;

    simple_platformer::Actor npc;
    npc.body.bounds = {{80.0F, 196.0F}, {12.0F, 12.0F}};
    npc.platformerMovement = simple_platformer::PlatformerMovement{};
    simple_platformer::NpcBrain brain;
    brain.state = simple_platformer::NpcState::Chase;
    npc.brain = brain;
    npc.senses = simple_platformer::NpcSenses{};
    npc.pathFollower = simple_platformer::PathFollower{};

    simple_platformer::World world;
    const simple_platformer::ActorId playerId = world.addActor(player);
    const simple_platformer::ActorId npcId = world.addActor(npc);
    world.setPlayer(playerId, {38.0F, 208.0F});
    const simple_platformer::TileMap map = tests::TileMapBuilder({"......", "######"});

    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};
    const simple_platformer::DebugOverlay debug =
        simple_platformer::makeDebugOverlay(world, map, cameraController, 128.0F);

    REQUIRE(debug.actors.size() == 2);
    const simple_platformer::ActorDebugInfo& playerDebug = debug.actors.front();
    REQUIRE(playerDebug.kind == simple_platformer::ActorDebugKind::Player);
    REQUIRE(playerDebug.animation == simple_platformer::AnimationName::Move);
    REQUIRE(playerDebug.sprite.has_value());
    const simple_platformer::ActorSpriteDebugInfo spriteDebug =
        playerDebug.sprite.value_or(simple_platformer::ActorSpriteDebugInfo{});
    REQUIRE(spriteDebug.bounds.position == glm::vec2{22.0F, 184.0F});
    REQUIRE(spriteDebug.bounds.size == glm::vec2{32.0F, 24.0F});
    REQUIRE(spriteDebug.atlasFrame == 7);
    REQUIRE(spriteDebug.atlasPosition == region.position);

    const simple_platformer::ActorDebugInfo& npcDebug = debug.actors.back();
    REQUIRE(npcDebug.id == npcId);
    REQUIRE(npcDebug.kind == simple_platformer::ActorDebugKind::Npc);
    REQUIRE(npcDebug.npcState == simple_platformer::NpcState::Chase);
    REQUIRE(npcDebug.pathFollower.has_value());
    const simple_platformer::PathFollowerDebugInfo emptyPath =
        npcDebug.pathFollower.value_or(simple_platformer::PathFollowerDebugInfo{});
    REQUIRE_FALSE(emptyPath.hasPath);
    REQUIRE(npcDebug.sensor.has_value());
    const simple_platformer::SensorDebugInfo emptySensor =
        npcDebug.sensor.value_or(simple_platformer::SensorDebugInfo{});
    REQUIRE_FALSE(emptySensor.visibleTargetCenter.has_value());
    REQUIRE_FALSE(emptySensor.rememberedTargetFeet.has_value());
}

TEST_CASE("Debug overlay data describes visible and remembered targets", "[app][debug]")
{
    simple_platformer::Actor player;
    player.body.bounds = {{48.0F, 20.0F}, {12.0F, 12.0F}};
    player.platformerMovement = simple_platformer::PlatformerMovement{};
    player.team = simple_platformer::Team::Player;

    simple_platformer::World world;
    const simple_platformer::ActorId playerId = world.addActor(player);
    world.setPlayer(playerId, {54.0F, 32.0F});

    simple_platformer::Actor visibleNpc;
    visibleNpc.body.bounds = {{16.0F, 20.0F}, {12.0F, 12.0F}};
    visibleNpc.platformerMovement = simple_platformer::PlatformerMovement{};
    visibleNpc.team = simple_platformer::Team::Enemy;
    visibleNpc.brain = simple_platformer::NpcBrain{};
    visibleNpc.brain->target = playerId;
    visibleNpc.brain->targetVisible = true;
    visibleNpc.brain->targetMemoryRemaining = 1.5F;
    visibleNpc.senses = simple_platformer::NpcSenses{80.0F, 1.5F};
    visibleNpc.pathFollower = simple_platformer::PathFollower{};
    world.addActor(visibleNpc);

    simple_platformer::Actor rememberedNpc = visibleNpc;
    rememberedNpc.body.bounds.position = {80.0F, 20.0F};
    rememberedNpc.brain->targetVisible = false;
    rememberedNpc.brain->lastSeenTargetFeet = {40.0F, 32.0F};
    rememberedNpc.brain->targetMemoryRemaining = 0.6F;
    world.addActor(rememberedNpc);

    const simple_platformer::TileMap map = tests::TileMapBuilder({".......", "#######"});
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};
    const simple_platformer::DebugOverlay debug =
        simple_platformer::makeDebugOverlay(world, map, cameraController, 128.0F);

    REQUIRE_FALSE(debug.actors[0].sensor.has_value());
    const simple_platformer::SensorDebugInfo visible =
        debug.actors[1].sensor.value_or(simple_platformer::SensorDebugInfo{});
    REQUIRE(visible.observerCenter == glm::vec2{22.0F, 26.0F});
    REQUIRE(visible.noticeDistance == 80.0F);
    REQUIRE(visible.visibleTargetCenter == glm::vec2{54.0F, 26.0F});
    REQUIRE_FALSE(visible.rememberedTargetFeet.has_value());

    const simple_platformer::SensorDebugInfo remembered =
        debug.actors[2].sensor.value_or(simple_platformer::SensorDebugInfo{});
    REQUIRE_FALSE(remembered.visibleTargetCenter.has_value());
    REQUIRE(remembered.rememberedTargetFeet == glm::vec2{40.0F, 32.0F});
    REQUIRE(remembered.memoryRemaining == 0.6F);
}

TEST_CASE("Debug overlay data describes projectiles", "[app][debug]")
{
    simple_platformer::World world;

    simple_platformer::Projectile owned;
    owned.bounds = {{24.0F, 32.0F}, {4.0F, 2.0F}};
    owned.remainingLifetime = 1.25F;
    owned.owner = simple_platformer::ActorId{7};
    owned.sprite.size = {4.0F, 2.0F};
    world.addProjectile(owned);

    simple_platformer::Projectile unowned = owned;
    unowned.bounds.position = {48.0F, 32.0F};
    unowned.remainingLifetime = 0.5F;
    unowned.owner = std::nullopt;
    world.addProjectile(unowned);

    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", "####"});
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};
    const simple_platformer::DebugOverlay debug =
        simple_platformer::makeDebugOverlay(world, map, cameraController, 128.0F);

    REQUIRE(debug.projectiles.size() == 2);
    REQUIRE(debug.projectiles[0].bounds.position == owned.bounds.position);
    REQUIRE(debug.projectiles[0].bounds.size == owned.bounds.size);
    REQUIRE(debug.projectiles[0].remainingLifetime == 1.25F);
    REQUIRE(debug.projectiles[0].owner == simple_platformer::ActorId{7});
    REQUIRE(debug.projectiles[1].remainingLifetime == 0.5F);
    REQUIRE_FALSE(debug.projectiles[1].owner.has_value());
}

TEST_CASE("Debug overlay data describes pickup bounds", "[app][debug]")
{
    simple_platformer::World world({{1, "Coin", {}, 5}});
    const simple_platformer::Aabb bounds{{24.0F, 32.0F}, {8.0F, 8.0F}};
    world.addPickup({bounds, {1, 2}});
    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", "####"});
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};

    const simple_platformer::DebugOverlay debug =
        simple_platformer::makeDebugOverlay(world, map, cameraController, 128.0F);

    REQUIRE(debug.pickups.size() == 1);
    REQUIRE(debug.pickups.front().bounds.position == bounds.position);
    REQUIRE(debug.pickups.front().bounds.size == bounds.size);
    REQUIRE(debug.pickups.front().itemName == "Coin");
}

TEST_CASE("Debug overlay data shows only an active bite hitbox", "[app][debug]")
{
    simple_platformer::Actor activeBiter;
    activeBiter.body.bounds = {{16.0F, 20.0F}, {12.0F, 12.0F}};
    activeBiter.platformerMovement = simple_platformer::PlatformerMovement{};
    activeBiter.facing = simple_platformer::Facing::Right;
    activeBiter.team = simple_platformer::Team::Enemy;
    activeBiter.bite = simple_platformer::BiteAttack{};
    activeBiter.bite->phase = simple_platformer::BitePhase::Active;
    activeBiter.bite->phaseTimeRemaining = 0.05F;

    simple_platformer::Actor recoveringBiter = activeBiter;
    recoveringBiter.body.bounds.position = {48.0F, 20.0F};
    recoveringBiter.bite->phase = simple_platformer::BitePhase::Recovery;

    simple_platformer::World world;
    world.addActor(activeBiter);
    world.addActor(recoveringBiter);
    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", "####"});
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};

    const simple_platformer::DebugOverlay debug =
        simple_platformer::makeDebugOverlay(world, map, cameraController, 128.0F);

    REQUIRE(debug.actors[0].biteHitbox.has_value());
    const simple_platformer::Aabb hitbox =
        debug.actors[0].biteHitbox.value_or(simple_platformer::Aabb{});
    REQUIRE(hitbox.position == glm::vec2{32.0F, 22.0F});
    REQUIRE(hitbox.size == glm::vec2{10.0F, 8.0F});
    REQUIRE_FALSE(debug.actors[1].biteHitbox.has_value());
}

TEST_CASE("Debug overlay data describes path connections and progress", "[app][debug]")
{
    simple_platformer::PathFollower follower;
    follower.path = simple_platformer::NavigationPath{
        {1, 2},
        {{{3, 2}, simple_platformer::Traversal::Walk, {}},
         {{4, 1}, simple_platformer::Traversal::Jump, {}},
         {{4, 3}, simple_platformer::Traversal::Fall, {}}}};
    follower.nextStep = 1;
    follower.destination = simple_platformer::GridPosition{4, 3};
    follower.repathRemaining = 0.12F;

    simple_platformer::Actor npc;
    npc.body.bounds = {{16.0F, 32.0F}, {12.0F, 12.0F}};
    npc.platformerMovement = simple_platformer::PlatformerMovement{};
    npc.brain = simple_platformer::NpcBrain{};
    npc.senses = simple_platformer::NpcSenses{};
    npc.pathFollower = follower;

    simple_platformer::World world;
    world.addActor(npc);
    const simple_platformer::TileMap map = tests::TileMapBuilder({"......", "######"});
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};

    const simple_platformer::DebugOverlay debug =
        simple_platformer::makeDebugOverlay(world, map, cameraController, 128.0F);

    REQUIRE(debug.actors.size() == 1);
    REQUIRE(debug.actors.front().pathFollower.has_value());
    const simple_platformer::PathFollowerDebugInfo path =
        debug.actors.front().pathFollower.value_or(simple_platformer::PathFollowerDebugInfo{});
    REQUIRE(path.hasPath);
    REQUIRE(path.nextStep == 1);
    REQUIRE(path.stepCount == 3);
    REQUIRE(path.destination == simple_platformer::GridPosition{4, 3});
    REQUIRE(path.repathRemaining == 0.12F);
    REQUIRE(path.connections.size() == 3);

    REQUIRE(path.connections[0].fromFeet == simple_platformer::navigationFeet({1, 2}));
    REQUIRE(path.connections[0].toFeet == simple_platformer::navigationFeet({3, 2}));
    REQUIRE(path.connections[0].traversal == simple_platformer::Traversal::Walk);
    REQUIRE(path.connections[0].completed);
    REQUIRE_FALSE(path.connections[0].next);

    REQUIRE(path.connections[1].fromFeet == simple_platformer::navigationFeet({3, 2}));
    REQUIRE(path.connections[1].toFeet == simple_platformer::navigationFeet({4, 1}));
    REQUIRE(path.connections[1].traversal == simple_platformer::Traversal::Jump);
    REQUIRE_FALSE(path.connections[1].completed);
    REQUIRE(path.connections[1].next);

    REQUIRE(path.connections[2].fromFeet == simple_platformer::navigationFeet({4, 1}));
    REQUIRE(path.connections[2].toFeet == simple_platformer::navigationFeet({4, 3}));
    REQUIRE(path.connections[2].traversal == simple_platformer::Traversal::Fall);
    REQUIRE_FALSE(path.connections[2].completed);
    REQUIRE_FALSE(path.connections[2].next);
}

TEST_CASE("Debug overlay data samples the simulated jump curve", "[app][debug]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const simple_platformer::PlatformerMovementConfig movementConfig;
    const std::vector<simple_platformer::NavigationNeighbor> neighbors =
        simple_platformer::platformerNeighbors(map, {2, 2}, {12.0F, 12.0F}, movementConfig);
    const auto jump = std::find_if(
        neighbors.begin(),
        neighbors.end(),
        [](const simple_platformer::NavigationNeighbor& neighbor)
        { return neighbor.traversal == simple_platformer::Traversal::Jump; });
    if (jump == neighbors.end())
    {
        throw std::logic_error("The test map did not produce a jump connection");
    }

    simple_platformer::Actor npc;
    npc.body.bounds = {{0.0F, 0.0F}, {12.0F, 12.0F}};
    npc.platformerMovement = simple_platformer::PlatformerMovement{movementConfig};
    npc.brain = simple_platformer::NpcBrain{};
    npc.senses = simple_platformer::NpcSenses{};
    npc.pathFollower = simple_platformer::PathFollower{
        simple_platformer::NavigationPath{
            {2, 2}, {{jump->destination, jump->traversal, jump->inputs}}},
        0,
        0.0F,
        jump->destination};

    simple_platformer::World world;
    world.addActor(npc);
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};

    const simple_platformer::DebugOverlay debug =
        simple_platformer::makeDebugOverlay(world, map, cameraController, 128.0F);
    const simple_platformer::PathFollowerDebugInfo path =
        debug.actors.front().pathFollower.value_or(simple_platformer::PathFollowerDebugInfo{});

    REQUIRE(path.connections.size() == 1);
    REQUIRE(path.connections.front().sampledFeet.size() > 2);
    const float takeoffY = simple_platformer::navigationFeet({2, 2}).y;
    const bool risesAboveTakeoff = std::any_of(
        path.connections.front().sampledFeet.begin(),
        path.connections.front().sampledFeet.end(),
        [takeoffY](glm::vec2 feet) { return feet.y < takeoffY; });
    REQUIRE(risesAboveTakeoff);
}

TEST_CASE("Debug overlay data rejects an invalid atlas width", "[app][debug]")
{
    const simple_platformer::World world;
    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", "####"});
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};

    REQUIRE_THROWS_AS(
        simple_platformer::makeDebugOverlay(world, map, cameraController, 0.0F),
        std::invalid_argument);
}
