#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "debug/debug_overlay.hpp"
#include "debug/navigation_debug.hpp"
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
#include "simple_platformer/npc/npc_state_machine.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/actor_builder.hpp"
#include "support/npc_facts_builder.hpp"
#include "support/npc_machine_builder.hpp"
#include "support/actor_components.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"
#include "support/add_player.hpp"
#include "support/fixed_step.hpp"
#include "support/neighbor_with.hpp"

TEST_CASE("Debug overlay data supports actors without presentation components", "[app][debug]")
{
    const simple_platformer::Actor actor =
        tests::ActorBuilder::sized({8.0F, 10.0F}).at({12.0F, 20.0F}).walking();
    simple_platformer::World world;
    const simple_platformer::ActorId id = world.addActor(actor);
    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", "####"});
    const simple_platformer::Camera camera{{4.0F, 5.0F}, simple_platformer::InternalViewportSize};
    const simple_platformer::CameraController cameraController{camera, {80.0F, 40.0F}};

    const simple_platformer::DebugOverlay debug = simple_platformer::makeDebugOverlay(
        world, map, cameraController, 128.0F, tests::FixedStepSeconds);

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

TEST_CASE("Debug overlay data marks a breakable tile under the cursor", "[app][debug]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..", "#g"})
            .where('g', tests::Tile().blocksMovement().breaksInto('.'));
    const simple_platformer::World world;
    const simple_platformer::CameraController cameraController{
        {{0.0F, 0.0F}, simple_platformer::InternalViewportSize}, {80.0F, 40.0F}};
    const auto overlayWithCursor = [&](std::optional<glm::vec2> cursor)
    {
        simple_platformer::NavigationDebugView view;
        view.cursorWorld = cursor;
        return simple_platformer::makeDebugOverlay(
            world, map, cameraController, 128.0F, tests::FixedStepSeconds, view);
    };

    REQUIRE_FALSE(overlayWithCursor(std::nullopt).breakableCellUnderCursor.has_value());
    // The solid tile does not break; the glass one does, and is marked by its cell.
    REQUIRE_FALSE(overlayWithCursor(glm::vec2{4.0F, 20.0F}).breakableCellUnderCursor.has_value());
    const std::optional<simple_platformer::Aabb> glass =
        overlayWithCursor(glm::vec2{20.0F, 20.0F}).breakableCellUnderCursor;
    REQUIRE(glass.has_value());
    REQUIRE(glass.value_or(simple_platformer::Aabb{}).position == glm::vec2{16.0F, 16.0F});
    REQUIRE(glass.value_or(simple_platformer::Aabb{}).size == glm::vec2{16.0F, 16.0F});
}

TEST_CASE("Debug overlay data describes NPC patrol points", "[app][debug]")
{
    simple_platformer::Actor npc = tests::ActorBuilder::sized({12.0F, 12.0F})
                                       .at({16.0F, 20.0F})
                                       .walking()
                                       .thinking({})
                                       .patrolling({24.0F, 32.0F}, {72.0F, 32.0F});
    tests::patrol(npc).headingToSecond = false;

    simple_platformer::World world;
    world.addActor(npc);
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", "#####"});
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};

    const simple_platformer::DebugOverlay debug = simple_platformer::makeDebugOverlay(
        world, map, cameraController, 128.0F, tests::FixedStepSeconds);

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

    const simple_platformer::Actor player = tests::ActorBuilder::sized({12.0F, 12.0F})
                                                .atFeet({38.0F, 208.0F})
                                                .walking()
                                                .withSprite({1, region, {32.0F, 24.0F}})
                                                .withAnimator(animator);

    simple_platformer::Actor npc =
        tests::ActorBuilder::sized({12.0F, 12.0F}).at({80.0F, 196.0F}).walking().thinking({});
    tests::brain(npc).state = simple_platformer::NpcState::Chase;

    simple_platformer::World world;
    tests::addPlayer(world, player);
    const simple_platformer::ActorId npcId = world.addActor(npc);
    const simple_platformer::TileMap map = tests::TileMapBuilder({"......", "######"});

    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{{0.0F, 100.0F}}, {80.0F, 40.0F}};
    const simple_platformer::DebugOverlay debug = simple_platformer::makeDebugOverlay(
        world, map, cameraController, 128.0F, tests::FixedStepSeconds);

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
    REQUIRE(npcDebug.npcTactic == simple_platformer::NpcTactic::Pursuer);
    REQUIRE_FALSE(npcDebug.machine.has_value());
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
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = tests::addPlayer(
        world,
        tests::ActorBuilder::sized({12.0F, 12.0F})
            .atFeet({54.0F, 32.0F})
            .walking()
            .onTeam(simple_platformer::Team::Player));

    simple_platformer::Actor visibleNpc = tests::ActorBuilder::sized({12.0F, 12.0F})
                                              .at({16.0F, 20.0F})
                                              .walking()
                                              .onTeam(simple_platformer::Team::Enemy)
                                              .thinking({80.0F, 1.5F});
    tests::brain(visibleNpc).target = playerId;
    tests::brain(visibleNpc).targetVisible = true;
    tests::brain(visibleNpc).targetMemoryRemaining = 1.5F;
    world.addActor(visibleNpc);

    simple_platformer::Actor rememberedNpc = visibleNpc;
    rememberedNpc.body.bounds.position = {80.0F, 20.0F};
    tests::brain(rememberedNpc).targetVisible = false;
    tests::brain(rememberedNpc).lastSeenTargetFeet = {40.0F, 32.0F};
    tests::brain(rememberedNpc).targetMemoryRemaining = 0.6F;
    world.addActor(rememberedNpc);

    const simple_platformer::TileMap map = tests::TileMapBuilder({".......", "#######"});
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};
    const simple_platformer::DebugOverlay debug = simple_platformer::makeDebugOverlay(
        world, map, cameraController, 128.0F, tests::FixedStepSeconds);

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
    owned.lifetimeRemaining = 1.25F;
    owned.owner = simple_platformer::ActorId{7};
    owned.sprite.size = {4.0F, 2.0F};
    world.addProjectile(owned);

    simple_platformer::Projectile unowned = owned;
    unowned.bounds.position = {48.0F, 32.0F};
    unowned.lifetimeRemaining = 0.5F;
    unowned.owner = std::nullopt;
    world.addProjectile(unowned);

    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", "####"});
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};
    const simple_platformer::DebugOverlay debug = simple_platformer::makeDebugOverlay(
        world, map, cameraController, 128.0F, tests::FixedStepSeconds);

    REQUIRE(debug.projectiles.size() == 2);
    REQUIRE(debug.projectiles[0].bounds.position == owned.bounds.position);
    REQUIRE(debug.projectiles[0].bounds.size == owned.bounds.size);
    REQUIRE(debug.projectiles[0].lifetimeRemaining == 1.25F);
    REQUIRE(debug.projectiles[0].owner == simple_platformer::ActorId{7});
    REQUIRE(debug.projectiles[1].lifetimeRemaining == 0.5F);
    REQUIRE_FALSE(debug.projectiles[1].owner.has_value());
}

TEST_CASE("Debug overlay data describes pickup bounds", "[app][debug]")
{
    simple_platformer::World world({{1, "Coin", {}, 5}});
    const simple_platformer::Aabb bounds{{24.0F, 32.0F}, {8.0F, 8.0F}};
    world.addPickup({{bounds}, {1, 2}});
    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", "####"});
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};

    const simple_platformer::DebugOverlay debug = simple_platformer::makeDebugOverlay(
        world, map, cameraController, 128.0F, tests::FixedStepSeconds);

    REQUIRE(debug.pickups.size() == 1);
    REQUIRE(debug.pickups.front().bounds.position == bounds.position);
    REQUIRE(debug.pickups.front().bounds.size == bounds.size);
    REQUIRE(debug.pickups.front().itemName == "Coin");
}

TEST_CASE("Debug overlay data shows only an active bite hitbox", "[app][debug]")
{
    simple_platformer::Actor activeBiter = tests::ActorBuilder::sized({12.0F, 12.0F})
                                               .at({16.0F, 20.0F})
                                               .walking()
                                               .onTeam(simple_platformer::Team::Enemy)
                                               .biting();
    activeBiter.facing = simple_platformer::Facing::Right;
    tests::bite(activeBiter).phase = simple_platformer::BitePhase::Active;
    tests::bite(activeBiter).phaseTimeRemaining = 0.05F;

    simple_platformer::Actor recoveringBiter = activeBiter;
    recoveringBiter.body.bounds.position = {48.0F, 20.0F};
    tests::bite(recoveringBiter).phase = simple_platformer::BitePhase::Recovery;

    simple_platformer::World world;
    world.addActor(activeBiter);
    world.addActor(recoveringBiter);
    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", "####"});
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};

    const simple_platformer::DebugOverlay debug = simple_platformer::makeDebugOverlay(
        world, map, cameraController, 128.0F, tests::FixedStepSeconds);

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
    follower.destinationCell = simple_platformer::GridPosition{4, 3};

    simple_platformer::Actor npc =
        tests::ActorBuilder::sized({12.0F, 12.0F}).at({16.0F, 32.0F}).walking().thinking({});
    npc.pathFollower = follower;

    simple_platformer::World world;
    world.addActor(npc);
    const simple_platformer::TileMap map = tests::TileMapBuilder({"......", "######"});
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};

    const simple_platformer::DebugOverlay debug = simple_platformer::makeDebugOverlay(
        world, map, cameraController, 128.0F, tests::FixedStepSeconds);

    REQUIRE(debug.actors.size() == 1);
    REQUIRE(debug.actors.front().pathFollower.has_value());
    const simple_platformer::PathFollowerDebugInfo path =
        debug.actors.front().pathFollower.value_or(simple_platformer::PathFollowerDebugInfo{});
    REQUIRE(path.hasPath);
    REQUIRE(path.nextStep == 1);
    REQUIRE(path.stepCount == 3);
    REQUIRE(path.destinationFeet == simple_platformer::feetInCell(tests::TileSize, {4, 3}));
    REQUIRE(path.connections.size() == 3);

    REQUIRE(path.connections[0].fromFeet == simple_platformer::feetInCell(tests::TileSize, {1, 2}));
    REQUIRE(path.connections[0].toFeet == simple_platformer::feetInCell(tests::TileSize, {3, 2}));
    REQUIRE(path.connections[0].traversal == simple_platformer::Traversal::Walk);
    REQUIRE(path.connections[0].completed);
    REQUIRE_FALSE(path.connections[0].next);

    REQUIRE(path.connections[1].fromFeet == simple_platformer::feetInCell(tests::TileSize, {3, 2}));
    REQUIRE(path.connections[1].toFeet == simple_platformer::feetInCell(tests::TileSize, {4, 1}));
    REQUIRE(path.connections[1].traversal == simple_platformer::Traversal::Jump);
    REQUIRE_FALSE(path.connections[1].completed);
    REQUIRE(path.connections[1].next);

    REQUIRE(path.connections[2].fromFeet == simple_platformer::feetInCell(tests::TileSize, {4, 1}));
    REQUIRE(path.connections[2].toFeet == simple_platformer::feetInCell(tests::TileSize, {4, 3}));
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
        simple_platformer::platformerNeighbors(
            map, {2, 2}, {12.0F, 12.0F}, movementConfig, tests::FixedStepSeconds);
    const simple_platformer::NavigationNeighbor& jump =
        tests::neighborWith(neighbors, simple_platformer::Traversal::Jump);

    simple_platformer::Actor npc = tests::ActorBuilder::sized({12.0F, 12.0F})
                                       .at({0.0F, 0.0F})
                                       .walking(movementConfig)
                                       .thinking({});
    npc.pathFollower = simple_platformer::PathFollower{
        simple_platformer::NavigationPath{
            {2, 2}, {{jump.destinationCell, jump.traversal, jump.inputs}}},
        0,
        0.0F,
        jump.destinationCell};

    simple_platformer::World world;
    world.addActor(npc);
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};

    const simple_platformer::DebugOverlay debug = simple_platformer::makeDebugOverlay(
        world, map, cameraController, 128.0F, tests::FixedStepSeconds);
    const simple_platformer::PathFollowerDebugInfo path =
        debug.actors.front().pathFollower.value_or(simple_platformer::PathFollowerDebugInfo{});

    REQUIRE(path.connections.size() == 1);
    REQUIRE(path.connections.front().sampledFeet.size() > 2);
    const float takeoffY = simple_platformer::feetInCell(tests::TileSize, {2, 2}).y;
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
        simple_platformer::makeDebugOverlay(
            world, map, cameraController, 0.0F, tests::FixedStepSeconds),
        std::invalid_argument);
}

TEST_CASE("The overlay shows a machine in place of the tactic it silences", "[app][debug]")
{
    simple_platformer::World world;
    world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                       .at({16.0F, 32.0F})
                       .walking()
                       .thinking({})
                       .running(tests::NpcMachineBuilder::named("test").state(
                           "rest", simple_platformer::NpcState::Idle)));
    const simple_platformer::TileMap map = tests::TileMapBuilder({"......", "######"});
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};

    const simple_platformer::DebugOverlay debug = simple_platformer::makeDebugOverlay(
        world, map, cameraController, 128.0F, tests::FixedStepSeconds);

    REQUIRE(debug.actors.size() == 1);
    REQUIRE(debug.actors.front().machine == "test");
    REQUIRE(debug.actors.front().machineState == "rest");
    REQUIRE_FALSE(debug.actors.front().npcTactic.has_value());
}

namespace
{
    // An NPC on the ground running a two-state machine, for the machine window's tests.
    simple_platformer::Actor machineNpc(glm::vec2 topLeft)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F})
            .at(topLeft)
            .walking()
            .thinking({})
            .running(tests::NpcMachineBuilder::named("test")
                         .state("rest", simple_platformer::NpcState::Idle)
                         .state("hunt", simple_platformer::NpcState::Chase)
                         .transition("rest", "hunt")
                         .when("targetKnown", true));
    }

    // Which NPC the machine window follows, or nothing.
    std::optional<simple_platformer::ActorId> followedBy(
        const simple_platformer::DebugOverlay& debug)
    {
        if (!debug.machine.has_value())
        {
            return std::nullopt;
        }
        return debug.machine.value_or(simple_platformer::MachineDebugInfo{}).actor;
    }

    simple_platformer::DebugOverlay overlayOf(
        const simple_platformer::World& world,
        std::optional<glm::vec2> cursorWorld = std::nullopt)
    {
        const simple_platformer::TileMap map = tests::TileMapBuilder({"......", "######"});
        const simple_platformer::CameraController cameraController{
            simple_platformer::Camera{}, {80.0F, 40.0F}};
        simple_platformer::NavigationDebugView view;
        view.cursorWorld = cursorWorld;
        return simple_platformer::makeDebugOverlay(
            world, map, cameraController, 128.0F, tests::FixedStepSeconds, view);
    }
}

TEST_CASE("The overlay shows only what the camera can see", "[app][debug]")
{
    simple_platformer::World world({{1, "Coin", {}, 5}});
    const glm::vec2 edge = simple_platformer::InternalViewportSize;
    const auto tile = static_cast<float>(tests::TileSize);
    const simple_platformer::ActorId beyondEdge = world.addActor(
        tests::ActorBuilder::sized({8.0F, 8.0F}).at({edge.x + tile * 0.5F, 20.0F}).walking());
    world.addActor(
        tests::ActorBuilder::sized({8.0F, 8.0F}).at({edge.x + tile * 2.0F, 20.0F}).walking());
    simple_platformer::Projectile shown;
    shown.bounds = {{20.0F, 20.0F}, {4.0F, 2.0F}};
    shown.lifetimeRemaining = 1.0F;
    shown.sprite.size = {4.0F, 2.0F};
    world.addProjectile(shown);
    simple_platformer::Projectile hidden = shown;
    hidden.bounds.position = {20.0F, edge.y + tile * 2.0F};
    world.addProjectile(hidden);
    world.addPickup({{{{20.0F, 40.0F}, {8.0F, 8.0F}}}, {1, 1}});
    world.addPickup({{{{20.0F, edge.y + tile * 2.0F}, {8.0F, 8.0F}}}, {1, 1}});

    const simple_platformer::DebugOverlay debug = overlayOf(world);

    // A tile beyond the camera's edge is still shown; further out is not.
    REQUIRE(debug.actors.size() == 1);
    REQUIRE(debug.actors.front().id == beyondEdge);
    REQUIRE(debug.projectiles.size() == 1);
    REQUIRE(debug.projectiles.front().bounds.position == shown.bounds.position);
    REQUIRE(debug.pickups.size() == 1);
    REQUIRE(debug.pickups.front().bounds.position == glm::vec2{20.0F, 40.0F});
}

TEST_CASE("The overlay shows only navigation cells near the camera", "[app][debug]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........................", "########################"});
    simple_platformer::World world;
    world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                       .atFeet({8.0F, 16.0F})
                       .walking()
                       .thinking({64.0F, 1.0F}));
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};

    const simple_platformer::DebugOverlay debug = simple_platformer::makeDebugOverlay(
        world, map, cameraController, 128.0F, tests::FixedStepSeconds);

    REQUIRE(debug.navigationCache.has_value());
    const std::vector<simple_platformer::NavigationCellDebugInfo>& cells =
        debug.navigationCache->cells;
    REQUIRE(cells.size() == 21);
    REQUIRE(cells.front().bounds.position == glm::vec2{0.0F, 0.0F});
    // As for actors, one tile beyond the camera's edge is shown; the rest are not.
    REQUIRE(cells.back().bounds.position == glm::vec2{320.0F, 0.0F});
}

TEST_CASE("The machine window follows the NPC with a machine nearest the player", "[app][debug]")
{
    simple_platformer::World world;
    tests::addPlayer(
        world, tests::ActorBuilder::sized({12.0F, 12.0F}).at({100.0F, 20.0F}).walking());
    // The nearest NPC has no machine, so the nearer of the two that do is followed.
    world.addActor(
        tests::ActorBuilder::sized({12.0F, 12.0F}).at({110.0F, 20.0F}).walking().thinking({}));
    world.addActor(machineNpc({200.0F, 20.0F}));
    const simple_platformer::ActorId nearer = world.addActor(machineNpc({60.0F, 20.0F}));

    const simple_platformer::DebugOverlay debug = overlayOf(world);

    REQUIRE(debug.machine.has_value());
    const simple_platformer::MachineDebugInfo machine =
        debug.machine.value_or(simple_platformer::MachineDebugInfo{});
    REQUIRE(machine.actor == nearer);
    REQUIRE(machine.definition.name == "test");
    REQUIRE(machine.definition.states.size() == 2);
    REQUIRE(machine.definition.transitions.size() == 1);
    REQUIRE(machine.active == 0);
    REQUIRE_FALSE(machine.lastFired.has_value());
}

TEST_CASE("The machine window follows the NPC under the cursor instead", "[app][debug]")
{
    simple_platformer::World world;
    tests::addPlayer(
        world, tests::ActorBuilder::sized({12.0F, 12.0F}).at({100.0F, 20.0F}).walking());
    world.addActor(machineNpc({60.0F, 20.0F}));
    const simple_platformer::ActorId further = world.addActor(machineNpc({200.0F, 20.0F}));

    REQUIRE(followedBy(overlayOf(world, glm::vec2{206.0F, 26.0F})) == further);
    // The cursor over an NPC without a machine, or over nothing, changes nothing.
    world.addActor(
        tests::ActorBuilder::sized({12.0F, 12.0F}).at({150.0F, 20.0F}).walking().thinking({}));
    REQUIRE(followedBy(overlayOf(world, glm::vec2{156.0F, 26.0F})) != further);
    REQUIRE(followedBy(overlayOf(world, glm::vec2{10.0F, 10.0F})) != further);
}

TEST_CASE("The machine window follows nothing off screen", "[app][debug]")
{
    simple_platformer::World world;
    tests::addPlayer(
        world, tests::ActorBuilder::sized({12.0F, 12.0F}).at({100.0F, 20.0F}).walking());
    world.addActor(machineNpc({simple_platformer::InternalViewportSize.x + 100.0F, 20.0F}));

    REQUIRE_FALSE(overlayOf(world).machine.has_value());
}

TEST_CASE("The machine window is told which transition fired last", "[app][debug]")
{
    simple_platformer::World world;
    const simple_platformer::ActorId id = world.addActor(machineNpc({60.0F, 20.0F}));
    REQUIRE(
        simple_platformer::advanceNpcMachine(
            tests::machine(world, id),
            tests::NpcFactsBuilder::facts().knowingTarget(),
            tests::FixedStepSeconds) == 0);

    const simple_platformer::DebugOverlay debug = overlayOf(world);

    REQUIRE(debug.machine.has_value());
    const simple_platformer::MachineDebugInfo machine =
        debug.machine.value_or(simple_platformer::MachineDebugInfo{});
    REQUIRE(machine.active == 1);
    REQUIRE(machine.lastFired == 0);
}
