#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"

namespace
{
    using Catch::Matchers::WithinAbs;
    using simple_platformer::Aabb;

    void requireVector(glm::vec2 actual, glm::vec2 expected)
    {
        REQUIRE_THAT(actual.x, WithinAbs(expected.x, 0.0001F));
        REQUIRE_THAT(actual.y, WithinAbs(expected.y, 0.0001F));
    }
}

TEST_CASE("An AABB position is its top-left corner", "[math][coordinates]")
{
    const Aabb box{{10.0F, 20.0F}, {12.0F, 24.0F}};

    requireVector(simple_platformer::centerOf(box), {16.0F, 32.0F});
    requireVector(simple_platformer::feetOf(box), {16.0F, 44.0F});
}

TEST_CASE("An arbitrary-sized AABB can be placed by its feet", "[math][coordinates]")
{
    Aabb box{{0.0F, 0.0F}, {12.0F, 24.0F}};

    simple_platformer::placeFeetAt(box, {40.0F, 128.0F});

    requireVector(box.position, {34.0F, 104.0F});
    requireVector(simple_platformer::feetOf(box), {40.0F, 128.0F});
}

TEST_CASE("AABBs overlap when their areas intersect", "[math][aabb]")
{
    const Aabb first{{10.0F, 10.0F}, {10.0F, 10.0F}};
    const Aabb second{{15.0F, 15.0F}, {10.0F, 10.0F}};

    REQUIRE(simple_platformer::overlaps(first, second));
    REQUIRE(simple_platformer::overlaps(second, first));
}

TEST_CASE("AABBs that only touch at an edge do not overlap", "[math][aabb]")
{
    const Aabb first{{10.0F, 10.0F}, {10.0F, 10.0F}};
    const Aabb second{{20.0F, 10.0F}, {10.0F, 10.0F}};

    REQUIRE_FALSE(simple_platformer::overlaps(first, second));
}

TEST_CASE("Grid positions compare by both coordinates", "[math][coordinates]")
{
    using simple_platformer::GridPosition;

    REQUIRE(GridPosition{2, 3} == GridPosition{2, 3});
    REQUIRE(GridPosition{2, 3} != GridPosition{3, 2});
}

TEST_CASE("World and grid coordinates convert at tile boundaries", "[math][coordinates]")
{
    using simple_platformer::GridPosition;

    REQUIRE(simple_platformer::worldToGrid({0.0F, 0.0F}) == GridPosition{0, 0});
    REQUIRE(simple_platformer::worldToGrid({15.9F, 31.9F}) == GridPosition{0, 1});
    REQUIRE(simple_platformer::worldToGrid({16.0F, 32.0F}) == GridPosition{1, 2});
    REQUIRE(simple_platformer::worldToGrid({-0.1F, -16.1F}) == GridPosition{-1, -2});
    requireVector(simple_platformer::gridToWorld({2, 3}), {32.0F, 48.0F});
}
