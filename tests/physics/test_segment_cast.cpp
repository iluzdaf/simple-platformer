#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <stdexcept>
#include <optional>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/physics/segment_cast.hpp"

TEST_CASE("A segment cast reports its first entry into an AABB", "[physics][segment]")
{
    const simple_platformer::Aabb box{{10.0F, 10.0F}, {10.0F, 10.0F}};
    const std::optional<float> hit =
        simple_platformer::segmentCast(box, {0.0F, 15.0F}, {40.0F, 15.0F});

    REQUIRE(hit.has_value());
    REQUIRE_THAT(hit.value_or(-1.0F), Catch::Matchers::WithinAbs(0.25F, 0.0001F));
}

TEST_CASE("A segment can miss or begin inside an AABB", "[physics][segment]")
{
    const simple_platformer::Aabb box{{10.0F, 10.0F}, {10.0F, 10.0F}};

    REQUIRE_FALSE(simple_platformer::segmentCast(box, {0.0F, 5.0F}, {40.0F, 5.0F}));
    REQUIRE(simple_platformer::segmentCast(box, {15.0F, 15.0F}, {40.0F, 15.0F}) == 0.0F);
}

TEST_CASE("Segment casts reject invalid data", "[physics][segment]")
{
    const simple_platformer::Aabb empty{{0.0F, 0.0F}, {0.0F, 10.0F}};
    REQUIRE_THROWS_AS(
        simple_platformer::segmentCast(empty, {0.0F, 0.0F}, {1.0F, 1.0F}), std::invalid_argument);
}
