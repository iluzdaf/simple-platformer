#include <catch2/catch_test_macros.hpp>

#include <limits>

#include <glm/vec2.hpp>

#include "simple_platformer/math/validation.hpp"

TEST_CASE("Vector finiteness checks both components", "[math][validation]")
{
    const float infinity = std::numeric_limits<float>::infinity();

    REQUIRE(simple_platformer::isFinite({1.0F, -2.0F}));
    REQUIRE_FALSE(simple_platformer::isFinite({infinity, 0.0F}));
    REQUIRE_FALSE(simple_platformer::isFinite({0.0F, infinity}));
}
