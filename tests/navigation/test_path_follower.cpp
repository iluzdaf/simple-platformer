#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/path_follower.hpp"

TEST_CASE("Navigation cells use feet on tile boundaries", "[navigation][path]")
{
    REQUIRE(
        simple_platformer::navigationCell({24.0F, 32.0F}) == simple_platformer::GridPosition{1, 1});
    REQUIRE(simple_platformer::navigationFeet({1, 1}) == glm::vec2{24.0F, 32.0F});
}

TEST_CASE("A flying path follower produces intentions for its next step", "[navigation][path]")
{
    simple_platformer::PathFollower follower;
    simple_platformer::setPath(follower, {{0, 0}, {1, 0}, {1, 1}}, {1, 1});
    simple_platformer::Aabb bounds{{4.0F, 4.0F}, {8.0F, 12.0F}};

    const simple_platformer::InputIntentions right =
        simple_platformer::followFlyingPath(bounds, follower);
    REQUIRE(right.direction.x == 1.0F);
    REQUIRE(right.direction.y == 0.0F);

    simple_platformer::placeFeetAt(bounds, simple_platformer::navigationFeet({1, 0}));
    const simple_platformer::InputIntentions down =
        simple_platformer::followFlyingPath(bounds, follower);
    REQUIRE(down.direction.x == 0.0F);
    REQUIRE(down.direction.y == 1.0F);

    simple_platformer::placeFeetAt(bounds, simple_platformer::navigationFeet({1, 1}));
    REQUIRE(simple_platformer::followFlyingPath(bounds, follower).direction == glm::vec2{0.0F});
    REQUIRE(simple_platformer::pathComplete(follower));
}
