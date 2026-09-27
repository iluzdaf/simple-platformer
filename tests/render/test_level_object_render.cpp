#include <catch2/catch_test_macros.hpp>

#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/render/render_scene.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/level_exit.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/require_near.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    std::vector<simple_platformer::ItemDefinition> items()
    {
        return {
            {1, "Coin", {}, 5},
            {2, "Potion", {}, 5, simple_platformer::ItemEffect::Heal, 2},
            {3, "Key", {}, 1}};
    }

    simple_platformer::Pickup pickupAt(
        glm::vec2 position,
        glm::vec2 size,
        simple_platformer::ItemStack stack)
    {
        simple_platformer::Pickup pickup;
        pickup.body.bounds = {position, size};
        pickup.stack = stack;
        return pickup;
    }
}

TEST_CASE("Pickups and exits produce camera-relative sprite commands", "[world][render][pickups]")
{
    auto definitions = items();
    definitions[0].icon = {7, {{4.0F, 8.0F}, {6.0F, 10.0F}}, {6.0F, 10.0F}};
    simple_platformer::World world(definitions);
    world.addPickup(pickupAt({20.0F, 20.0F}, {12.0F, 16.0F}, {1, 1}));
    world.advanceSimulationTime(0.5F);
    simple_platformer::LevelExit exit;
    exit.bounds = {{50.0F, 20.0F}, {16.0F, 32.0F}};
    exit.sprite = simple_platformer::Sprite{8, {{8.0F, 8.0F}, {16.0F, 32.0F}}, {16.0F, 32.0F}};
    world.setExit(exit);
    const simple_platformer::TileMap map = tests::TileMapBuilder({"......", "......", "......"});
    simple_platformer::Camera camera;
    camera.position = {10.0F, 5.0F};
    const auto scene = simple_platformer::buildRenderScene(map, 0, camera, world);
    REQUIRE(scene.sprites.size() == 2);
    REQUIRE(scene.sprites[0].textureId == 7);
    REQUIRE(scene.sprites[0].position.x == 13.0F);
    REQUIRE(scene.sprites[0].position.y == 21.0F);
    REQUIRE(scene.sprites[1].textureId == 8);
    REQUIRE(scene.sprites[1].position.x == 40.0F);
}

TEST_CASE(
    "Pickup sprites use position-based bobbing without moving their bounds",
    "[world][render][pickups]")
{
    auto definitions = items();
    definitions[0].icon = {7, {{4.0F, 8.0F}, {6.0F, 10.0F}}, {6.0F, 10.0F}};
    simple_platformer::World world(definitions);
    world.addPickup(pickupAt({0.0F, 0.0F}, {16.0F, 16.0F}, {1, 1}));
    world.addPickup(pickupAt({16.0F, 0.0F}, {16.0F, 16.0F}, {1, 1}));
    const simple_platformer::TileMap map = tests::TileMapBuilder({"......", "......", "......"});
    const simple_platformer::Camera camera{{0.0F, 0.0F}, simple_platformer::InternalViewportSize};

    const auto initialScene = simple_platformer::buildRenderScene(map, 0, camera, world);
    world.advanceSimulationTime(0.5F);
    const auto advancedScene = simple_platformer::buildRenderScene(map, 0, camera, world);

    REQUIRE_NEAR(initialScene.sprites[0].position.y, 6.0F);
    REQUIRE_NEAR(initialScene.sprites[1].position.y, 5.0F);
    REQUIRE_NEAR(advancedScene.sprites[0].position.y, 4.0F);
    REQUIRE_NEAR(advancedScene.sprites[1].position.y, 5.0F);
    REQUIRE(world.pickups()[0].body.bounds.position == glm::vec2{0.0F, 0.0F});
    REQUIRE(world.pickups()[1].body.bounds.position == glm::vec2{16.0F, 0.0F});
}

TEST_CASE("Pickup sprite overrides leave inventory icons unchanged", "[world][render][pickups]")
{
    simple_platformer::World world(items());
    const simple_platformer::Sprite sprite{
        7, {{24, 8}, {12, 10}}, {24, 20}, simple_platformer::SpriteAnchor::BodyCenter};
    simple_platformer::Pickup pickup = pickupAt({20.0F, 20.0F}, {8.0F, 8.0F}, {1, 1});
    pickup.sprite = sprite;
    world.addPickup(pickup);
    const simple_platformer::TileMap map = tests::TileMapBuilder({"......", "......", "......"});
    const auto scene = simple_platformer::buildRenderScene(map, 0, {}, world);
    REQUIRE(scene.sprites.size() == 1);
    REQUIRE(scene.sprites[0].textureId == 7);
    REQUIRE(scene.sprites[0].source.position == glm::vec2{24, 8});
    REQUIRE(scene.sprites[0].size == glm::vec2{24, 20});
    REQUIRE(scene.sprites[0].position.x == 12);
    REQUIRE(world.itemDefinition(1).icon.textureId != 7);
    REQUIRE(world.pickups()[0].body.bounds.size == glm::vec2{8, 8});
}
