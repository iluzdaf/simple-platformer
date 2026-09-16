#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace
{
    using Catch::Matchers::WithinAbs;
    using simple_platformer::Aabb;
    using simple_platformer::Camera;
    using simple_platformer::TileMap;

    TileMap makeMap(int width, int height)
    {
        return {
            width,
            height,
            std::vector<int>(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0),
            {{false}}};
    }

    void requireVector(glm::vec2 actual, glm::vec2 expected)
    {
        REQUIRE_THAT(actual.x, WithinAbs(expected.x, 0.0001F));
        REQUIRE_THAT(actual.y, WithinAbs(expected.y, 0.0001F));
    }
}

TEST_CASE("The camera locks to the target centre", "[render][camera]")
{
    const TileMap map = makeMap(30, 20);
    const Aabb target{{145.0F, 95.0F}, {10.0F, 10.0F}};

    const Camera camera = simple_platformer::makeLockedCamera(map, target, {100.0F, 60.0F});

    requireVector(camera.position, {100.0F, 70.0F});
    requireVector(simple_platformer::worldToScreen(camera, target.position), {45.0F, 25.0F});
    requireVector(simple_platformer::screenToWorld(camera, {45.0F, 25.0F}), target.position);
}

TEST_CASE("The camera clamps to every map edge", "[render][camera]")
{
    const TileMap map = makeMap(30, 20);

    SECTION("top left")
    {
        const Camera camera = simple_platformer::makeLockedCamera(
            map, {{0.0F, 0.0F}, {10.0F, 10.0F}}, {100.0F, 60.0F});
        requireVector(camera.position, {0.0F, 0.0F});
    }

    SECTION("bottom right")
    {
        const Camera camera = simple_platformer::makeLockedCamera(
            map, {{470.0F, 310.0F}, {10.0F, 10.0F}}, {100.0F, 60.0F});
        requireVector(camera.position, {380.0F, 260.0F});
    }
}

TEST_CASE("Maps smaller than the viewport are centred", "[render][camera]")
{
    const TileMap map = makeMap(4, 3);

    const Camera camera =
        simple_platformer::makeLockedCamera(map, {{20.0F, 20.0F}, {10.0F, 10.0F}});

    requireVector(camera.position, {-128.0F, -66.0F});
}

TEST_CASE("Camera movement is rounded to internal pixels", "[render][camera]")
{
    const TileMap map = makeMap(30, 20);
    const Aabb target{{155.6F, 70.4F}, {10.0F, 10.0F}};

    const Camera camera = simple_platformer::makeLockedCamera(map, target, {100.0F, 60.0F});

    requireVector(camera.position, {111.0F, 45.0F});
}

TEST_CASE("Camera viewports must have positive finite dimensions", "[render][camera]")
{
    const TileMap map = makeMap(4, 3);

    REQUIRE_THROWS_AS(
        simple_platformer::makeLockedCamera(map, {{0.0F, 0.0F}, {10.0F, 10.0F}}, {0.0F, 180.0F}),
        std::invalid_argument);
}
