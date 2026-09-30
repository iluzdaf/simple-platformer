#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "content/game_catalogs.hpp"
#include "content/level_catalog.hpp"
#include "debug/debug_overlay.hpp"
#include "debug/navigation_debug.hpp"
#include "game/game.hpp"
#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"
#include "support/add_player.hpp"
#include "support/fixed_step.hpp"

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

    REQUIRE(debug.cameraBounds.topLeft == camera.position);
    REQUIRE(debug.cameraBounds.size == camera.viewportSize);
    REQUIRE(debug.cameraDeadZone.topLeft == glm::vec2{124.0F, 75.0F});
    REQUIRE(debug.cameraDeadZone.size == cameraController.deadZoneSize);
    REQUIRE(debug.actors.size() == 1);
    REQUIRE(debug.actors.front().id == id);
    REQUIRE(debug.actors.front().kind == simple_platformer::ActorDebugKind::Actor);
    REQUIRE(debug.actors.front().collider.topLeft == actor.body.bounds.topLeft);
    REQUIRE(debug.actors.front().collider.size == actor.body.bounds.size);
    REQUIRE_FALSE(debug.actors.front().sprite.has_value());
    REQUIRE_FALSE(debug.actors.front().animation.has_value());
    REQUIRE_FALSE(debug.actors.front().npcState.has_value());
    REQUIRE_FALSE(debug.actors.front().pathFollower.has_value());
    REQUIRE_FALSE(debug.actors.front().sensor.has_value());
    REQUIRE_FALSE(debug.actors.front().patrol.has_value());
    REQUIRE_FALSE(debug.actors.front().biteHitbox.has_value());
}

TEST_CASE("Game debug data retains actor definition names", "[app][debug]")
{
    const auto levels = simple_platformer::parseLevelCatalog(
        R"({"startLevel":1,"levels":[{"number":1,"file":"actor_placement.json"}]})",
        "test catalog",
        "tests/fixtures/levels");
    simple_platformer::Game game(
        0,
        levels,
        simple_platformer::loadGameCatalogs("tests/fixtures/catalogs"),
        simple_platformer::LuaNpcScripts{},
        tests::FixedStepSeconds);

    const simple_platformer::DebugOverlay debug = game.debugOverlay(128.0F, std::nullopt, 0);
    const auto npc = std::find_if(
        debug.actors.begin(),
        debug.actors.end(),
        [](const simple_platformer::ActorDebugInfo& actor)
        { return actor.kind == simple_platformer::ActorDebugKind::Npc; });
    REQUIRE(npc != debug.actors.end());
    REQUIRE(npc->definitionName == "test_guard");

    const auto player = std::find_if(
        debug.actors.begin(),
        debug.actors.end(),
        [](const simple_platformer::ActorDebugInfo& actor)
        { return actor.kind == simple_platformer::ActorDebugKind::Player; });
    REQUIRE(player != debug.actors.end());
    REQUIRE(player->definitionName == "test_player");
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
    REQUIRE(glass.value_or(simple_platformer::Aabb{}).topLeft == glm::vec2{16.0F, 16.0F});
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
    REQUIRE(spriteDebug.bounds.topLeft == glm::vec2{22.0F, 184.0F});
    REQUIRE(spriteDebug.bounds.size == glm::vec2{32.0F, 24.0F});
    REQUIRE(spriteDebug.atlasFrame == 7);
    REQUIRE(spriteDebug.atlasPosition == region.position);

    const simple_platformer::ActorDebugInfo& npcDebug = debug.actors.back();
    REQUIRE(npcDebug.id == npcId);
    REQUIRE(npcDebug.kind == simple_platformer::ActorDebugKind::Npc);
    REQUIRE(npcDebug.npcState == simple_platformer::NpcState::Chase);
    REQUIRE(npcDebug.npcTactic == simple_platformer::NpcTactic::Pursuer);
    REQUIRE_FALSE(npcDebug.machineState.has_value());
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
    tests::perception(visibleNpc).targetVisible = true;
    tests::brain(visibleNpc).targetMemoryRemaining = 1.5F;
    world.addActor(visibleNpc);

    simple_platformer::Actor rememberedNpc = visibleNpc;
    rememberedNpc.body.bounds.topLeft = {80.0F, 20.0F};
    tests::perception(rememberedNpc).targetVisible = false;
    tests::brain(rememberedNpc).lastKnownTargetFeet = {40.0F, 32.0F};
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
    unowned.bounds.topLeft = {48.0F, 32.0F};
    unowned.lifetimeRemaining = 0.5F;
    unowned.owner = std::nullopt;
    world.addProjectile(unowned);

    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", "####"});
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};
    const simple_platformer::DebugOverlay debug = simple_platformer::makeDebugOverlay(
        world, map, cameraController, 128.0F, tests::FixedStepSeconds);

    REQUIRE(debug.projectiles.size() == 2);
    REQUIRE(debug.projectiles[0].bounds.topLeft == owned.bounds.topLeft);
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
    REQUIRE(debug.pickups.front().bounds.topLeft == bounds.topLeft);
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
    recoveringBiter.body.bounds.topLeft = {48.0F, 20.0F};
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
    REQUIRE(hitbox.topLeft == glm::vec2{32.0F, 22.0F});
    REQUIRE(hitbox.size == glm::vec2{10.0F, 8.0F});
    REQUIRE_FALSE(debug.actors[1].biteHitbox.has_value());
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
    hidden.bounds.topLeft = {20.0F, edge.y + tile * 2.0F};
    world.addProjectile(hidden);
    world.addPickup({{{{20.0F, 40.0F}, {8.0F, 8.0F}}}, {1, 1}});
    world.addPickup({{{{20.0F, edge.y + tile * 2.0F}, {8.0F, 8.0F}}}, {1, 1}});

    const simple_platformer::TileMap map = tests::TileMapBuilder({"......", "######"});
    const simple_platformer::CameraController cameraController{
        simple_platformer::Camera{}, {80.0F, 40.0F}};
    const simple_platformer::DebugOverlay debug = simple_platformer::makeDebugOverlay(
        world, map, cameraController, 128.0F, tests::FixedStepSeconds);

    // A tile beyond the camera's edge is still shown; further out is not.
    REQUIRE(debug.actors.size() == 1);
    REQUIRE(debug.actors.front().id == beyondEdge);
    REQUIRE(debug.projectiles.size() == 1);
    REQUIRE(debug.projectiles.front().bounds.topLeft == shown.bounds.topLeft);
    REQUIRE(debug.pickups.size() == 1);
    REQUIRE(debug.pickups.front().bounds.topLeft == glm::vec2{20.0F, 40.0F});
}
