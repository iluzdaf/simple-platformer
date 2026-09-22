#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/render/cover_fade.hpp"
#include "simple_platformer/render/render_scene.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/level_exit.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/require_near.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/add_player.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"

namespace
{
    // Each kind of thing draws from its own texture, so a scene can be searched for it.
    constexpr int PlayerTexture = 1;
    constexpr int NpcTexture = 2;
    constexpr int PickupTexture = 3;
    constexpr int TileTexture = 7;

    std::size_t spritesFrom(const simple_platformer::RenderScene& scene, int textureId)
    {
        return static_cast<std::size_t>(std::count_if(
            scene.sprites.begin(),
            scene.sprites.end(),
            [textureId](const simple_platformer::SpriteDrawCommand& sprite)
            { return sprite.textureId == textureId; }));
    }

    const simple_platformer::SpriteDrawCommand& onlySpriteFrom(
        const simple_platformer::RenderScene& scene,
        int textureId)
    {
        REQUIRE(spritesFrom(scene, textureId) == 1);
        return *std::find_if(
            scene.sprites.begin(),
            scene.sprites.end(),
            [textureId](const simple_platformer::SpriteDrawCommand& sprite)
            { return sprite.textureId == textureId; });
    }

    simple_platformer::Sprite square(int textureId, float size)
    {
        return {textureId, {{0.0F, 0.0F}, {size, size}}, {size, size}};
    }

    simple_platformer::World worldWithPickupItem()
    {
        simple_platformer::ItemDefinition coin;
        coin.id = 1;
        coin.name = "coin";
        coin.icon = square(PickupTexture, 8.0F);
        coin.maximumStack = 9;
        return simple_platformer::World({coin});
    }

    void addPlayerIn(simple_platformer::World& world, simple_platformer::GridPosition cell)
    {
        tests::addPlayer(
            world,
            tests::ActorBuilder::sized({12.0F, 12.0F})
                .inCell(cell)
                .walking()
                .withSprite(square(PlayerTexture, 12.0F)));
    }

    void addNpcIn(simple_platformer::World& world, simple_platformer::GridPosition cell)
    {
        world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                           .inCell(cell)
                           .flying(0.0F)
                           .withSprite(square(NpcTexture, 12.0F)));
    }
}

TEST_CASE("A render scene contains visible tiles followed by the player", "[render][scene]")
{
    const simple_platformer::SpriteRegion tileRegion{{5.0F, 6.0F}, {1.0F, 1.0F}};
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({".xxx", "...."})
            .where('x', tests::Tile().blocksMovement().blocksSight().withSprite(tileRegion));
    const simple_platformer::Camera camera{{16.0F, 0.0F}, {32.0F, 16.0F}};
    const simple_platformer::Sprite player{9, {{2.0F, 0.0F}, {1.0F, 1.0F}}, {10.0F, 14.0F}};
    const simple_platformer::Aabb playerBounds{{20.0F, 2.0F}, {8.0F, 12.0F}};
    simple_platformer::Actor actor = tests::ActorBuilder::sized(playerBounds.size)
                                         .at(playerBounds.position)
                                         .walking()
                                         .withSprite(player);
    actor.facing = simple_platformer::Facing::Left;
    simple_platformer::World world;
    tests::addPlayer(world, actor);

    const simple_platformer::RenderScene scene =
        simple_platformer::buildRenderScene(map, 7, camera, world);

    REQUIRE(scene.sprites.size() == 3);

    REQUIRE(scene.sprites[0].textureId == 7);
    REQUIRE(scene.sprites[0].position.x == 0.0F);
    REQUIRE(scene.sprites[0].source.position.x == 5.0F);
    REQUIRE_FALSE(scene.sprites[0].flipHorizontal);

    REQUIRE(scene.sprites[1].position.x == 16.0F);

    REQUIRE(scene.sprites[2].textureId == 9);
    REQUIRE(scene.sprites[2].position.x == 3.0F);
    REQUIRE(scene.sprites[2].position.y == 0.0F);
    REQUIRE(scene.sprites[2].size.x == 10.0F);
    REQUIRE(scene.sprites[2].flipHorizontal);
}

TEST_CASE("Tile rendering includes non-solid tiles and preserves each region", "[render][scene]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({".ab"})
            .where('a', tests::Tile().withSprite({{16, 0}, {16, 16}}))
            .where(
                'b', tests::Tile().blocksMovement().blocksSight().withSprite({{32, 0}, {16, 16}}));
    const auto scene =
        simple_platformer::buildRenderScene(map, 7, {{0, 0}, {48, 16}}, simple_platformer::World{});
    REQUIRE(scene.sprites.size() == 2);
    REQUIRE(scene.sprites[0].source.position.x == 16);
    REQUIRE(scene.sprites[1].source.position.x == 32);
    REQUIRE(scene.sprites[0].size == glm::vec2{16, 16});
    REQUIRE(scene.sprites[1].size == glm::vec2{16, 16});
}

TEST_CASE("Facing right does not flip the player sprite", "[render][scene]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"..", "##"});
    const simple_platformer::Camera camera{{0.0F, 0.0F}, {32.0F, 32.0F}};
    const simple_platformer::Sprite player{1, {{1.0F, 0.0F}, {1.0F, 1.0F}}, {12.0F, 12.0F}};
    simple_platformer::World world;
    world.addActor(
        tests::ActorBuilder::sized({12.0F, 12.0F}).at({4.0F, 4.0F}).walking().withSprite(player));

    const simple_platformer::RenderScene scene =
        simple_platformer::buildRenderScene(map, 1, camera, world);

    REQUIRE_FALSE(scene.sprites.back().flipHorizontal);
}

TEST_CASE("A centre-anchored sprite surrounds a smaller flying body", "[render][scene]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"..."});
    const simple_platformer::Camera camera{{0.0F, 0.0F}, {48.0F, 32.0F}};
    simple_platformer::World world;
    world.addActor(tests::ActorBuilder::sized({12.0F, 8.0F})
                       .at({10.0F, 10.0F})
                       .flying(0.0F)
                       .withSprite(
                           {1,
                            {{0.0F, 96.0F}, {32.0F, 24.0F}},
                            {32.0F, 24.0F},
                            simple_platformer::SpriteAnchor::BodyCenter}));

    const simple_platformer::RenderScene scene =
        simple_platformer::buildRenderScene(map, 1, camera, world);

    REQUIRE(scene.sprites.size() == 1);
    REQUIRE(scene.sprites.front().position == glm::vec2{0.0F, 2.0F});
    REQUIRE(scene.sprites.front().size == glm::vec2{32.0F, 24.0F});
}

TEST_CASE("Actors without sprites do not produce draw commands", "[render][scene]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".."});
    const simple_platformer::Camera camera{{0.0F, 0.0F}, {32.0F, 16.0F}};
    simple_platformer::World world;
    world.addActor(tests::ActorBuilder::sized({8.0F, 8.0F}).at({4.0F, 4.0F}).walking());

    const simple_platformer::RenderScene scene =
        simple_platformer::buildRenderScene(map, 1, camera, world);

    REQUIRE(scene.sprites.empty());
}

TEST_CASE("Dying actors fade during the final part of their death", "[render][scene]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".."});
    const simple_platformer::Camera camera{{0.0F, 0.0F}, {32.0F, 16.0F}};
    simple_platformer::World world;
    world.addActor(tests::ActorBuilder::sized({8.0F, 8.0F})
                       .at({4.0F, 4.0F})
                       .walking()
                       .withSprite({1, {{0.0F, 0.0F}, {8.0F, 8.0F}}, {8.0F, 8.0F}}));

    const auto aliveScene = simple_platformer::buildRenderScene(map, 1, camera, world);
    REQUIRE(aliveScene.sprites.back().opacity == 1.0F);

    world.actors().front().life = simple_platformer::LifeState::Dying;
    world.actors().front().deathTimeRemaining = 0.3F;
    const auto earlyDeathScene = simple_platformer::buildRenderScene(map, 1, camera, world);
    REQUIRE(earlyDeathScene.sprites.back().opacity == 1.0F);

    world.actors().front().deathTimeRemaining = 0.1F;
    const auto lateDeathScene = simple_platformer::buildRenderScene(map, 1, camera, world);
    REQUIRE_NEAR(lateDeathScene.sprites.back().opacity, 0.5F);
}

TEST_CASE("Actors with active hit feedback produce a white flash", "[render][scene]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".."});
    const simple_platformer::Camera camera{{0.0F, 0.0F}, {32.0F, 16.0F}};
    simple_platformer::Actor actor =
        tests::ActorBuilder::sized({8.0F, 8.0F})
            .at({4.0F, 4.0F})
            .walking()
            .withSprite({1, {{0.0F, 0.0F}, {8.0F, 8.0F}}, {8.0F, 8.0F}});
    actor.lastDamageTimeSeconds = 0.0F;
    simple_platformer::World world;
    world.addActor(actor);
    world.advanceSimulationTime(0.05F);

    const auto scene = simple_platformer::buildRenderScene(map, 1, camera, world);
    REQUIRE(scene.sprites.back().whiteFlashAmount > 0.0F);

    world.advanceSimulationTime(0.05F);
    const auto laterScene = simple_platformer::buildRenderScene(map, 1, camera, world);
    REQUIRE(laterScene.sprites.back().whiteFlashAmount == 0.0F);
}

TEST_CASE("Projectile sprites are centred and rotated in their direction", "[render][scene]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", "...."});
    const simple_platformer::Camera camera{{0.0F, 0.0F}, {64.0F, 32.0F}};
    simple_platformer::Projectile projectile;
    projectile.bounds = {{20.0F, 10.0F}, {4.0F, 2.0F}};
    projectile.velocity = {0.0F, -10.0F};
    projectile.sprite = {3, {{2.0F, 0.0F}, {1.0F, 1.0F}}, {6.0F, 4.0F}};
    simple_platformer::World world;
    world.addProjectile(projectile);

    const simple_platformer::RenderScene scene =
        simple_platformer::buildRenderScene(map, 0, camera, world);

    REQUIRE(scene.sprites.size() == 1);
    REQUIRE(scene.sprites.front().position.x == 19.0F);
    REQUIRE(scene.sprites.front().position.y == 9.0F);
    REQUIRE(scene.sprites.front().size.x == 6.0F);
    REQUIRE_FALSE(scene.sprites.front().flipHorizontal);
    REQUIRE_NEAR(scene.sprites.front().rotationRadians, -std::acos(-1.0F) * 0.5F);
}

TEST_CASE("Projectile bursts expand and fade around their world position", "[render][scene]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", "...."});
    const simple_platformer::Camera camera{{0.0F, 0.0F}, {64.0F, 32.0F}};
    simple_platformer::ProjectileBurst burst;
    burst.center = {20.0F, 10.0F};
    burst.direction = {0.0F, -1.0F};
    burst.sprite = {3, {{2.0F, 0.0F}, {1.0F, 1.0F}}, {6.0F, 4.0F}};
    burst.lifetimeRemaining = 0.05F;
    simple_platformer::World world;
    world.addProjectileBurst(burst);

    const simple_platformer::RenderScene scene =
        simple_platformer::buildRenderScene(map, 0, camera, world);

    REQUIRE(scene.sprites.size() == 1);
    REQUIRE_NEAR(scene.sprites.front().position.x, 15.5F);
    REQUIRE_NEAR(scene.sprites.front().position.y, 7.0F);
    REQUIRE_NEAR(scene.sprites.front().size.x, 9.0F);
    REQUIRE_NEAR(scene.sprites.front().size.y, 6.0F);
    REQUIRE_NEAR(scene.sprites.front().opacity, 0.5F);
    REQUIRE_NEAR(scene.sprites.front().rotationRadians, -std::acos(-1.0F) * 0.5F);
}

TEST_CASE("NPCs and pickups the player cannot see are not drawn", "[render][scene][cover]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "...cc...", "........"})
            .where('c', tests::Tile().blocksSight());
    const simple_platformer::Camera camera{{0.0F, 0.0F}, {128.0F, 48.0F}};
    simple_platformer::World world = worldWithPickupItem();
    addPlayerIn(world, {0, 1});
    addNpcIn(world, {4, 1});
    world.addPickup(
        {{simple_platformer::boxInCell(tests::TileSize, {3, 1}, {8.0F, 8.0F})}, {1, 1}});
    addNpcIn(world, {7, 1});
    simple_platformer::updateCoverFades(map, world, 0.0F);

    const simple_platformer::RenderScene scene =
        simple_platformer::buildRenderScene(map, TileTexture, camera, world);

    REQUIRE(spritesFrom(scene, PlayerTexture) == 1);
    // Only the NPC in the open.
    REQUIRE(spritesFrom(scene, NpcTexture) == 1);
    REQUIRE(spritesFrom(scene, PickupTexture) == 0);
}

TEST_CASE("NPCs in the player's own patch of cover are drawn fully", "[render][scene][cover]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "...cc...", "........"})
            .where('c', tests::Tile().blocksSight());
    const simple_platformer::Camera camera{{0.0F, 0.0F}, {128.0F, 48.0F}};
    simple_platformer::World world;
    addPlayerIn(world, {3, 1});
    addNpcIn(world, {4, 1});
    simple_platformer::updateCoverFades(map, world, 0.0F);

    const simple_platformer::RenderScene scene =
        simple_platformer::buildRenderScene(map, TileTexture, camera, world);

    REQUIRE(onlySpriteFrom(scene, NpcTexture).opacity == 1.0F);
}

TEST_CASE("NPCs and pickups are drawn at their screen visibility", "[render][scene][cover]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", "....", "...."});
    const simple_platformer::Camera camera{{0.0F, 0.0F}, {64.0F, 48.0F}};
    simple_platformer::World world = worldWithPickupItem();
    addPlayerIn(world, {0, 1});
    addNpcIn(world, {2, 1});
    world.addPickup(
        {{simple_platformer::boxInCell(tests::TileSize, {3, 1}, {8.0F, 8.0F})}, {1, 1}});
    world.actors().back().screenVisibility = 0.4F;
    world.pickups().front().screenVisibility = 0.4F;

    const simple_platformer::RenderScene partlyShown =
        simple_platformer::buildRenderScene(map, TileTexture, camera, world);
    REQUIRE_NEAR(onlySpriteFrom(partlyShown, NpcTexture).opacity, 0.4F);
    REQUIRE_NEAR(onlySpriteFrom(partlyShown, PickupTexture).opacity, 0.4F);

    world.actors().back().screenVisibility = 0.0F;
    world.pickups().front().screenVisibility = 0.0F;
    const simple_platformer::RenderScene hidden =
        simple_platformer::buildRenderScene(map, TileTexture, camera, world);
    REQUIRE(spritesFrom(hidden, NpcTexture) == 0);
    REQUIRE(spritesFrom(hidden, PickupTexture) == 0);
}

TEST_CASE("The player is shaded by how concealed they are, never faded", "[render][scene][cover]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", "....", "...."});
    const simple_platformer::Camera camera{{0.0F, 0.0F}, {64.0F, 48.0F}};
    simple_platformer::World world;
    addPlayerIn(world, {0, 1});
    addNpcIn(world, {2, 1});
    tests::player(world).screenVisibility = 0.0F;
    world.actors().back().screenVisibility = 0.5F;

    const simple_platformer::RenderScene scene =
        simple_platformer::buildRenderScene(map, TileTexture, camera, world);

    const simple_platformer::SpriteDrawCommand& player = onlySpriteFrom(scene, PlayerTexture);
    REQUIRE(player.opacity == 1.0F);
    REQUIRE_NEAR(player.shadeAmount, simple_platformer::PlayerConcealedShade);
    const simple_platformer::SpriteDrawCommand& npc = onlySpriteFrom(scene, NpcTexture);
    REQUIRE_NEAR(npc.opacity, 0.5F);
    REQUIRE(npc.shadeAmount == 0.0F);

    tests::player(world).screenVisibility = 1.0F;
    const simple_platformer::RenderScene exposed =
        simple_platformer::buildRenderScene(map, TileTexture, camera, world);
    REQUIRE(onlySpriteFrom(exposed, PlayerTexture).shadeAmount == 0.0F);
}

TEST_CASE("The player fades into the exit and the door flashes while it opens", "[render][scene]")
{
    constexpr int DoorTexture = 4;
    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", "....", "...."});
    const simple_platformer::Camera camera{{0.0F, 0.0F}, {64.0F, 48.0F}};
    simple_platformer::World world;
    addPlayerIn(world, {1, 1});
    simple_platformer::LevelExit exit;
    exit.bounds = simple_platformer::boxInCell(tests::TileSize, {1, 1}, {16.0F, 32.0F});
    exit.sprite = square(DoorTexture, 16.0F);
    world.setExit(exit);

    const auto closed = simple_platformer::buildRenderScene(map, TileTexture, camera, world);
    REQUIRE(onlySpriteFrom(closed, PlayerTexture).opacity == 1.0F);
    REQUIRE(onlySpriteFrom(closed, DoorTexture).whiteFlashAmount == 0.0F);

    exit.openedTimeSeconds = 0.0F;
    world.setExit(exit);
    world.advanceSimulationTime(simple_platformer::ExitOpenSeconds * 0.5F);
    const auto halfOpen = simple_platformer::buildRenderScene(map, TileTexture, camera, world);
    REQUIRE_NEAR(onlySpriteFrom(halfOpen, PlayerTexture).opacity, 0.5F);
    REQUIRE_NEAR(onlySpriteFrom(halfOpen, DoorTexture).whiteFlashAmount, 0.25F);

    world.advanceSimulationTime(simple_platformer::ExitOpenSeconds * 0.5F);
    const auto open = simple_platformer::buildRenderScene(map, TileTexture, camera, world);
    REQUIRE(onlySpriteFrom(open, PlayerTexture).opacity == 0.0F);
    REQUIRE(onlySpriteFrom(open, DoorTexture).whiteFlashAmount == 0.0F);
}
