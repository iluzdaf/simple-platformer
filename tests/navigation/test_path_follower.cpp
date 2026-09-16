#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/timing/fixed_step.hpp"
#include "simple_platformer/world/tile_map.hpp"

TEST_CASE("Navigation cells use feet on tile boundaries", "[navigation][path]")
{
    REQUIRE(
        simple_platformer::navigationCell({24.0F, 32.0F}) == simple_platformer::GridPosition{1, 1});
    REQUIRE(simple_platformer::navigationFeet({1, 1}) == glm::vec2{24.0F, 32.0F});
}

TEST_CASE("A flying path follower produces intentions for its next step", "[navigation][path]")
{
    simple_platformer::PathFollower follower;
    simple_platformer::setPath(
        follower,
        {{0, 0},
         {{{1, 0}, simple_platformer::Traversal::Fly, {}},
          {{1, 1}, simple_platformer::Traversal::Fly, {}}}},
        {1, 1});
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

TEST_CASE(
    "A platformer path follower executes a generated jump through movement and collision",
    "[navigation][path][integration]")
{
    const simple_platformer::TileMap map = simple_platformer::TileMap::fromAscii(
        {"..........", "....##....", "..........", "##########"});
    const glm::vec2 bodySize{12.0F, 12.0F};
    const simple_platformer::PlatformerMovementConfig config;
    const std::vector<simple_platformer::NavigationNeighbor> neighbors =
        simple_platformer::platformerNeighbors(map, {2, 2}, bodySize, config);
    const auto jump = std::find_if(
        neighbors.begin(),
        neighbors.end(),
        [](const simple_platformer::NavigationNeighbor& neighbor)
        { return neighbor.traversal == simple_platformer::Traversal::Jump; });
    if (jump == neighbors.end())
    {
        throw std::logic_error("The test level did not produce a jump");
    }

    simple_platformer::PathFollower follower;
    simple_platformer::setPath(
        follower,
        {{2, 2}, {{jump->destination, jump->traversal, jump->inputs}}},
        jump->destination);
    simple_platformer::Body body{{{0.0F, 0.0F}, bodySize}, {0.0F, 0.0F}};
    simple_platformer::placeFeetAt(body.bounds, simple_platformer::navigationFeet({2, 2}));
    simple_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};
    simple_platformer::Facing facing = simple_platformer::Facing::Right;
    constexpr float DeltaTime = static_cast<float>(simple_platformer::FixedDeltaSeconds);

    for (int tick = 0; tick < 180 && !simple_platformer::pathComplete(follower); ++tick)
    {
        const simple_platformer::InputIntentions intentions =
            simple_platformer::followPlatformerPath(body, movement, follower, DeltaTime);
        simple_platformer::updatePlatformerMovement(
            map, body, movement, intentions, facing, DeltaTime);
    }

    REQUIRE(simple_platformer::pathComplete(follower));
    REQUIRE(
        simple_platformer::navigationCell(simple_platformer::feetOf(body.bounds)) ==
        jump->destination);
}

TEST_CASE(
    "A platformer path follower approaches and brakes without moving the body directly",
    "[navigation][path][integration]")
{
    const simple_platformer::TileMap map = simple_platformer::TileMap::fromAscii(
        {"..........", "....##....", "..........", "##########"});
    const glm::vec2 bodySize{12.0F, 12.0F};
    const simple_platformer::PlatformerMovementConfig config;
    const std::vector<simple_platformer::NavigationNeighbor> neighbors =
        simple_platformer::platformerNeighbors(map, {2, 2}, bodySize, config);
    const auto jump = std::find_if(
        neighbors.begin(),
        neighbors.end(),
        [](const simple_platformer::NavigationNeighbor& neighbor)
        { return neighbor.traversal == simple_platformer::Traversal::Jump; });
    if (jump == neighbors.end())
    {
        throw std::logic_error("The test level did not produce a jump");
    }

    simple_platformer::PathFollower follower;
    simple_platformer::setPath(
        follower,
        {{2, 2}, {{jump->destination, jump->traversal, jump->inputs}}},
        jump->destination);
    simple_platformer::Body body{{{0.0F, 0.0F}, bodySize}, {80.0F, 0.0F}};
    simple_platformer::placeFeetAt(body.bounds, simple_platformer::navigationFeet({2, 2}));
    body.bounds.position.x -= 6.0F;
    simple_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};
    simple_platformer::Facing facing = simple_platformer::Facing::Right;
    constexpr float DeltaTime = static_cast<float>(simple_platformer::FixedDeltaSeconds);
    bool preparedForJump = false;

    for (int tick = 0; tick < 240 && !simple_platformer::pathComplete(follower); ++tick)
    {
        const glm::vec2 positionBeforeFollowing = body.bounds.position;
        const glm::vec2 velocityBeforeFollowing = body.velocity;
        const simple_platformer::InputIntentions intentions =
            simple_platformer::followPlatformerPath(body, movement, follower, DeltaTime);
        preparedForJump = preparedForJump || follower.programElapsed == 0.0F;

        REQUIRE(body.bounds.position == positionBeforeFollowing);
        REQUIRE(body.velocity == velocityBeforeFollowing);
        simple_platformer::updatePlatformerMovement(
            map, body, movement, intentions, facing, DeltaTime);
    }

    REQUIRE(preparedForJump);
    REQUIRE(simple_platformer::pathComplete(follower));
    REQUIRE(
        simple_platformer::navigationCell(simple_platformer::feetOf(body.bounds)) ==
        jump->destination);
}
