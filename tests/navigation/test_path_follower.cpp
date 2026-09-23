#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/require_near.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"
#include "support/fixed_step.hpp"
#include "support/neighbor_with.hpp"

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
    const simple_platformer::FlyingMovement movement;

    const simple_platformer::InputIntentions right = simple_platformer::followFlyingPath(
        tests::TileSize, bounds, movement, follower, tests::FixedStepSeconds);
    REQUIRE(right.direction.x == 1.0F);
    REQUIRE(right.direction.y == 0.0F);

    bounds = simple_platformer::boxInCell(tests::TileSize, {1, 0}, bounds.size);
    const simple_platformer::InputIntentions down = simple_platformer::followFlyingPath(
        tests::TileSize, bounds, movement, follower, tests::FixedStepSeconds);
    REQUIRE(down.direction.x == 0.0F);
    REQUIRE(down.direction.y == 1.0F);

    bounds = simple_platformer::boxInCell(tests::TileSize, {1, 1}, bounds.size);
    REQUIRE(
        simple_platformer::followFlyingPath(
            tests::TileSize, bounds, movement, follower, tests::FixedStepSeconds)
            .direction == glm::vec2{0.0F});
    REQUIRE(simple_platformer::pathComplete(follower));
}

TEST_CASE("A flying path follower uses the exact remaining waypoint distance", "[navigation][path]")
{
    simple_platformer::PathFollower follower;
    simple_platformer::setPath(
        follower, {{0, 0}, {{{1, 0}, simple_platformer::Traversal::Fly, {}}}}, {1, 0});
    simple_platformer::Aabb bounds{{19.75F, 4.0F}, {8.0F, 12.0F}};
    const simple_platformer::FlyingMovement movement{60.0F};

    const simple_platformer::InputIntentions intentions = simple_platformer::followFlyingPath(
        tests::TileSize, bounds, movement, follower, tests::FixedStepSeconds);

    REQUIRE_NEAR(intentions.direction.x, 0.25F);
    REQUIRE(intentions.direction.y == 0.0F);
    REQUIRE_FALSE(simple_platformer::pathComplete(follower));
}

TEST_CASE(
    "A platformer path follower executes a generated jump through movement and collision",
    "[navigation][path][integration]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const glm::vec2 bodySize{12.0F, 12.0F};
    const simple_platformer::PlatformerMovementConfig config;
    const std::vector<simple_platformer::NavigationNeighbor> neighbors =
        simple_platformer::platformerNeighbors(
            map, {2, 2}, bodySize, config, tests::FixedStepSeconds);
    const simple_platformer::NavigationNeighbor& jump =
        tests::neighborWith(neighbors, simple_platformer::Traversal::Jump);

    simple_platformer::PathFollower follower;
    simple_platformer::setPath(
        follower,
        {{2, 2}, {{jump.destinationCell, jump.traversal, jump.inputs}}},
        jump.destinationCell);
    simple_platformer::Body body{
        simple_platformer::boxInCell(tests::TileSize, {2, 2}, bodySize), {0.0F, 0.0F}};
    simple_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};

    for (int tick = 0; tick < 180 && !simple_platformer::pathComplete(follower); ++tick)
    {
        const simple_platformer::InputIntentions intentions =
            simple_platformer::followPlatformerPath(
                tests::TileSize, body, movement, follower, tests::FixedStepSeconds);
        simple_platformer::updatePlatformerMovement(
            map, body, movement, intentions, tests::FixedStepSeconds);
    }

    REQUIRE(simple_platformer::pathComplete(follower));
    REQUIRE(
        simple_platformer::cellAtFeet(tests::TileSize, simple_platformer::feetOf(body.bounds)) ==
        jump.destinationCell);
}

TEST_CASE(
    "A platformer path follower approaches and brakes without moving the body directly",
    "[navigation][path][integration]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const glm::vec2 bodySize{12.0F, 12.0F};
    const simple_platformer::PlatformerMovementConfig config;
    const std::vector<simple_platformer::NavigationNeighbor> neighbors =
        simple_platformer::platformerNeighbors(
            map, {2, 2}, bodySize, config, tests::FixedStepSeconds);
    const simple_platformer::NavigationNeighbor& jump =
        tests::neighborWith(neighbors, simple_platformer::Traversal::Jump);

    simple_platformer::PathFollower follower;
    simple_platformer::setPath(
        follower,
        {{2, 2}, {{jump.destinationCell, jump.traversal, jump.inputs}}},
        jump.destinationCell);
    simple_platformer::Body body{
        simple_platformer::boxInCell(tests::TileSize, {2, 2}, bodySize), {80.0F, 0.0F}};
    body.bounds.position.x -= 6.0F;
    simple_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};
    bool preparedForJump = false;

    for (int tick = 0; tick < 240 && !simple_platformer::pathComplete(follower); ++tick)
    {
        const glm::vec2 positionBeforeFollowing = body.bounds.position;
        const glm::vec2 velocityBeforeFollowing = body.velocity;
        const simple_platformer::InputIntentions intentions =
            simple_platformer::followPlatformerPath(
                tests::TileSize, body, movement, follower, tests::FixedStepSeconds);
        preparedForJump = preparedForJump || follower.programElapsed == 0.0F;

        REQUIRE(body.bounds.position == positionBeforeFollowing);
        REQUIRE(body.velocity == velocityBeforeFollowing);
        simple_platformer::updatePlatformerMovement(
            map, body, movement, intentions, tests::FixedStepSeconds);
    }

    REQUIRE(preparedForJump);
    REQUIRE(simple_platformer::pathComplete(follower));
    REQUIRE(
        simple_platformer::cellAtFeet(tests::TileSize, simple_platformer::feetOf(body.bounds)) ==
        jump.destinationCell);
}

TEST_CASE(
    "A platformer path follower brakes between a walk and a generated jump",
    "[navigation][path][integration]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const glm::vec2 bodySize{12.0F, 12.0F};
    const simple_platformer::PlatformerMovementConfig config;
    const std::vector<simple_platformer::NavigationNeighbor> neighbors =
        simple_platformer::platformerNeighbors(
            map, {2, 2}, bodySize, config, tests::FixedStepSeconds);
    const simple_platformer::NavigationNeighbor& jump =
        tests::neighborWith(neighbors, simple_platformer::Traversal::Jump);

    simple_platformer::PathFollower follower;
    simple_platformer::setPath(
        follower,
        {{1, 2},
         {{{2, 2}, simple_platformer::Traversal::Walk, {}},
          {jump.destinationCell, jump.traversal, jump.inputs}}},
        jump.destinationCell);
    simple_platformer::Body body{
        simple_platformer::boxInCell(tests::TileSize, {1, 2}, bodySize), {0.0F, 0.0F}};
    simple_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};
    bool brakedAfterWalking = false;

    for (int tick = 0; tick < 360 && !simple_platformer::pathComplete(follower); ++tick)
    {
        const simple_platformer::InputIntentions intentions =
            simple_platformer::followPlatformerPath(
                tests::TileSize, body, movement, follower, tests::FixedStepSeconds);
        brakedAfterWalking =
            brakedAfterWalking ||
            (follower.nextStep == 0 && body.velocity.x != 0.0F && intentions.direction.x == 0.0F);
        simple_platformer::updatePlatformerMovement(
            map, body, movement, intentions, tests::FixedStepSeconds);
    }

    REQUIRE(brakedAfterWalking);
    REQUIRE(simple_platformer::pathComplete(follower));
    REQUIRE(
        simple_platformer::cellAtFeet(tests::TileSize, simple_platformer::feetOf(body.bounds)) ==
        jump.destinationCell);
}
