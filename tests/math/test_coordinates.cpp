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

TEST_CASE("Grid positions compare by both coordinates", "[math][coordinates]")
{
    using simple_platformer::GridPosition;

    REQUIRE(GridPosition{2, 3} == GridPosition{2, 3});
    REQUIRE(GridPosition{2, 3} != GridPosition{3, 2});
}
