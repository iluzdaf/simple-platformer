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
    REQUIRE_NOTHROW(simple_platformer::requireSeconds(0.0F, "Attacks time step"));
    REQUIRE_NOTHROW(simple_platformer::requireSeconds(0.25F, "Attacks time step"));
    REQUIRE_THROWS_WITH(
        simple_platformer::requireSeconds(-0.1F, "Attacks time step"),
        "Attacks time step must be a finite, non-negative number of seconds");
    REQUIRE_THROWS_AS(
        simple_platformer::requireSeconds(
            std::numeric_limits<float>::infinity(), "Attacks time step"),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::requireSeconds(
            std::numeric_limits<float>::quiet_NaN(), "Attacks time step"),
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

TEST_CASE("A positive time step rejects zero and non-finite values", "[math][validation]")
{
    REQUIRE_NOTHROW(simple_platformer::requirePositiveSeconds(0.25F, "Navigation simulation step"));
    REQUIRE_THROWS_WITH(
        simple_platformer::requirePositiveSeconds(0.0F, "Navigation simulation step"),
        "Navigation simulation step must be finite and positive");
    REQUIRE_THROWS_AS(
        simple_platformer::requirePositiveSeconds(-0.1F, "Navigation simulation step"),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::requirePositiveSeconds(
            std::numeric_limits<float>::infinity(), "Navigation simulation step"),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::requirePositiveSeconds(
            std::numeric_limits<float>::quiet_NaN(), "Navigation simulation step"),
        std::invalid_argument);
}

TEST_CASE("A length of time must be finite and not negative", "[math][validation]")
{
    REQUIRE_NOTHROW(simple_platformer::requireSeconds(0.0F, "Frame time"));
    REQUIRE_THROWS_WITH(
        simple_platformer::requireSeconds(-1.0F, "Frame time"),
        "Frame time must be a finite, non-negative number of seconds");
    REQUIRE_THROWS_AS(
        simple_platformer::requireSeconds(std::numeric_limits<float>::infinity(), "Frame time"),
        std::invalid_argument);
}
