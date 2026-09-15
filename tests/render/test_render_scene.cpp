#include <catch2/catch_test_macros.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/render/render_scene.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

TEST_CASE("A render scene contains visible tiles followed by the player", "[render][scene]")
{
    const simple_platformer::SpriteRegion tileRegion{{5.0F, 6.0F}, {1.0F, 1.0F}};
    const simple_platformer::TileMap map(
        4, 2, {0, 1, 1, 1, 0, 0, 0, 0}, {{false}, {true, tileRegion}});
    const simple_platformer::Camera camera{{16.0F, 0.0F}, {32.0F, 16.0F}};
    const simple_platformer::Sprite player{9, {{2.0F, 0.0F}, {1.0F, 1.0F}}, {10.0F, 14.0F}};
    const simple_platformer::Aabb playerBounds{{20.0F, 2.0F}, {8.0F, 12.0F}};
    simple_platformer::Actor actor;
    actor.body.bounds = playerBounds;
    actor.platformerMovement = simple_platformer::PlatformerMovement{};
    actor.facing = simple_platformer::Facing::Left;
    actor.sprite = player;
    simple_platformer::World world;
    world.addActor(actor);

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

TEST_CASE("Facing right does not flip the player sprite", "[render][scene]")
{
    const simple_platformer::TileMap map = simple_platformer::TileMap::fromAscii({"..", "##"});
    const simple_platformer::Camera camera{{0.0F, 0.0F}, {32.0F, 32.0F}};
    const simple_platformer::Sprite player{1, {{1.0F, 0.0F}, {1.0F, 1.0F}}, {12.0F, 12.0F}};
    simple_platformer::Actor actor;
    actor.body = {{{4.0F, 4.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    actor.platformerMovement = simple_platformer::PlatformerMovement{};
    actor.sprite = player;
    simple_platformer::World world;
    world.addActor(actor);

    const simple_platformer::RenderScene scene =
        simple_platformer::buildRenderScene(map, 1, camera, world);

    REQUIRE_FALSE(scene.sprites.back().flipHorizontal);
}

TEST_CASE("Actors without sprites do not produce draw commands", "[render][scene]")
{
    const simple_platformer::TileMap map = simple_platformer::TileMap::fromAscii({".."});
    const simple_platformer::Camera camera{{0.0F, 0.0F}, {32.0F, 16.0F}};
    simple_platformer::Actor actor;
    actor.body = {{{4.0F, 4.0F}, {8.0F, 8.0F}}, {0.0F, 0.0F}};
    actor.platformerMovement = simple_platformer::PlatformerMovement{};
    simple_platformer::World world;
    world.addActor(actor);

    const simple_platformer::RenderScene scene =
        simple_platformer::buildRenderScene(map, 1, camera, world);

    REQUIRE(scene.sprites.empty());
}
