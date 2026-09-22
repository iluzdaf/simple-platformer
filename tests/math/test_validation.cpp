#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <limits>
#include <stdexcept>

#include <glm/vec2.hpp>

#include "simple_platformer/math/validation.hpp"

TEST_CASE("Vector finiteness checks both components", "[math][validation]")
{
    const float infinity = std::numeric_limits<float>::infinity();

    REQUIRE(simple_platformer::isFinite({1.0F, -2.0F}));
    REQUIRE_FALSE(simple_platformer::isFinite({infinity, 0.0F}));
    REQUIRE_FALSE(simple_platformer::isFinite({0.0F, infinity}));
}

TEST_CASE("A time step must be finite and not negative", "[math][validation]")
{
    REQUIRE_NOTHROW(simple_platformer::requireTimeStep(0.0F, "Steps"));
    REQUIRE_NOTHROW(simple_platformer::requireTimeStep(0.25F, "Steps"));
    REQUIRE_THROWS_WITH(
        simple_platformer::requireTimeStep(-0.1F, "Steps"),
        Catch::Matchers::ContainsSubstring("Steps require"));
    REQUIRE_THROWS_AS(
        simple_platformer::requireTimeStep(std::numeric_limits<float>::infinity(), "Steps"),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::requireTimeStep(std::numeric_limits<float>::quiet_NaN(), "Steps"),
        std::invalid_argument);
}

TEST_CASE("A finite positive number is above zero and not infinite", "[math][validation]")
{
    REQUIRE(simple_platformer::isFinitePositive(0.5F));
    REQUIRE_FALSE(simple_platformer::isFinitePositive(0.0F));
    REQUIRE_FALSE(simple_platformer::isFinitePositive(-0.5F));
    REQUIRE_FALSE(simple_platformer::isFinitePositive(std::numeric_limits<float>::infinity()));
    REQUIRE_FALSE(simple_platformer::isFinitePositive(std::numeric_limits<float>::quiet_NaN()));
}
