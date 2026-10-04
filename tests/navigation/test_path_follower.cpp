#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/route.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/platformer_connections.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/navigation/traversal.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/require_near.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"
#include "support/fixed_step.hpp"
#include "support/navigation_paths.hpp"
#include "support/route_connections.hpp"

TEST_CASE("A flying path follower produces intentions for its next step", "[navigation][follower]")
{
    simple_platformer::PathFollower follower;
    simple_platformer::setPath(
        follower,
        tests::floorPath(
            {0, 0},
            {{{{1, 0}}, simple_platformer::Traversal::Fly, {}},
             {{{1, 1}}, simple_platformer::Traversal::Fly, {}}}));
    simple_platformer::Aabb bounds{{4.0F, 4.0F}, {8.0F, 12.0F}};
    const simple_platformer::FlyingMovement movement;

    const simple_platformer::InputIntentions right =
        simple_platformer::followFlyingPath(bounds, movement, follower, tests::FixedStepSeconds);
    REQUIRE(right.direction.x == 1.0F);
    REQUIRE(right.direction.y == 0.0F);

    bounds = simple_platformer::boxInCell(tests::TileSize, {1, 0}, bounds.size);
    const simple_platformer::InputIntentions down =
        simple_platformer::followFlyingPath(bounds, movement, follower, tests::FixedStepSeconds);
    REQUIRE(down.direction.x == 0.0F);
    REQUIRE(down.direction.y == 1.0F);

    bounds = simple_platformer::boxInCell(tests::TileSize, {1, 1}, bounds.size);
    REQUIRE(
        simple_platformer::followFlyingPath(bounds, movement, follower, tests::FixedStepSeconds)
            .direction == glm::vec2{0.0F});
    REQUIRE(simple_platformer::pathComplete(follower));
}

TEST_CASE(
    "A flying path follower uses the exact remaining waypoint distance",
    "[navigation][follower]")
{
    simple_platformer::PathFollower follower;
    simple_platformer::setPath(
        follower, tests::floorPath({0, 0}, {{{{1, 0}}, simple_platformer::Traversal::Fly, {}}}));
    simple_platformer::Aabb bounds{{19.75F, 4.0F}, {8.0F, 12.0F}};
    const simple_platformer::FlyingMovement movement{60.0F};

    const simple_platformer::InputIntentions intentions =
        simple_platformer::followFlyingPath(bounds, movement, follower, tests::FixedStepSeconds);

    REQUIRE_NEAR(intentions.direction.x, 0.25F);
    REQUIRE(intentions.direction.y == 0.0F);
    REQUIRE_FALSE(simple_platformer::pathComplete(follower));
}

TEST_CASE(
    "A platformer path follower executes a generated jump through movement and collision",
    "[navigation][follower]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const glm::vec2 bodySize{12.0F, 12.0F};
    const simple_platformer::PlatformerMovementConfig config;
    const std::vector<simple_platformer::RouteConnection> connections =
        simple_platformer::buildPlatformerConnections(
            map,
            {2, 2},
            simple_platformer::PlatformerTraversalProfile{
                bodySize, config, tests::FixedStepSeconds})
            .connections;
    const simple_platformer::RouteConnection& jump =
        tests::connectionWith(connections, simple_platformer::Traversal::Jump);

    simple_platformer::PathFollower follower;
    simple_platformer::setPath(follower, tests::floorPath({2, 2}, {jump.step}));
    simple_platformer::Body body{
        simple_platformer::boxInCell(tests::TileSize, {2, 2}, bodySize), {0.0F, 0.0F}};
    simple_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};

    for (int tick = 0; tick < 180 && !simple_platformer::pathComplete(follower); ++tick)
    {
        const simple_platformer::InputIntentions intentions =
            simple_platformer::followPlatformerPath(
                body, movement, follower, tests::FixedStepSeconds);
        simple_platformer::updatePlatformerMovement(
            map, body, movement, intentions, tests::FixedStepSeconds);
    }

    REQUIRE(simple_platformer::pathComplete(follower));
    REQUIRE(
        simple_platformer::cellAtFeet(tests::TileSize, simple_platformer::feetOf(body.bounds)) ==
        jump.step.destination.cell);
}

TEST_CASE(
    "A platformer path follower approaches and brakes without moving the body directly",
    "[navigation][follower]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const glm::vec2 bodySize{12.0F, 12.0F};
    const simple_platformer::PlatformerMovementConfig config;
    const std::vector<simple_platformer::RouteConnection> connections =
        simple_platformer::buildPlatformerConnections(
            map,
            {2, 2},
            simple_platformer::PlatformerTraversalProfile{
                bodySize, config, tests::FixedStepSeconds})
            .connections;
    const simple_platformer::RouteConnection& jump =
        tests::connectionWith(connections, simple_platformer::Traversal::Jump);

    simple_platformer::PathFollower follower;
    simple_platformer::setPath(follower, tests::floorPath({2, 2}, {jump.step}));
    simple_platformer::Body body{
        simple_platformer::boxInCell(tests::TileSize, {2, 2}, bodySize), {80.0F, 0.0F}};
    body.bounds.topLeft.x -= 6.0F;
    simple_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};
    bool preparedForJump = false;

    for (int tick = 0; tick < 240 && !simple_platformer::pathComplete(follower); ++tick)
    {
        const glm::vec2 positionBeforeFollowing = body.bounds.topLeft;
        const glm::vec2 velocityBeforeFollowing = body.velocity;
        const simple_platformer::InputIntentions intentions =
            simple_platformer::followPlatformerPath(
                body, movement, follower, tests::FixedStepSeconds);
        preparedForJump =
            preparedForJump || follower.phase == simple_platformer::PathStepPhase::ApproachStart;

        REQUIRE(body.bounds.topLeft == positionBeforeFollowing);
        REQUIRE(body.velocity == velocityBeforeFollowing);
        simple_platformer::updatePlatformerMovement(
            map, body, movement, intentions, tests::FixedStepSeconds);
    }

    REQUIRE(preparedForJump);
    REQUIRE(simple_platformer::pathComplete(follower));
    REQUIRE(
        simple_platformer::cellAtFeet(tests::TileSize, simple_platformer::feetOf(body.bounds)) ==
        jump.step.destination.cell);
}

TEST_CASE(
    "A platformer path follower brakes between a walk and a generated jump",
    "[navigation][follower]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const glm::vec2 bodySize{12.0F, 12.0F};
    const simple_platformer::PlatformerMovementConfig config;
    const std::vector<simple_platformer::RouteConnection> connections =
        simple_platformer::buildPlatformerConnections(
            map,
            {2, 2},
            simple_platformer::PlatformerTraversalProfile{
                bodySize, config, tests::FixedStepSeconds})
            .connections;
    const simple_platformer::RouteConnection& jump =
        tests::connectionWith(connections, simple_platformer::Traversal::Jump);

    simple_platformer::PathFollower follower;
    simple_platformer::setPath(
        follower,
        tests::floorPath({1, 2}, {{{{2, 2}}, simple_platformer::Traversal::Walk, {}}, jump.step}));
    simple_platformer::Body body{
        simple_platformer::boxInCell(tests::TileSize, {1, 2}, bodySize), {0.0F, 0.0F}};
    simple_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};
    bool brakedAfterWalking = false;

    for (int tick = 0; tick < 360 && !simple_platformer::pathComplete(follower); ++tick)
    {
        const simple_platformer::InputIntentions intentions =
            simple_platformer::followPlatformerPath(
                body, movement, follower, tests::FixedStepSeconds);
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
        jump.step.destination.cell);
}

TEST_CASE(
    "A platformer path follower drops a path whose jump lands on another row",
    "[navigation][follower]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const glm::vec2 bodySize{12.0F, 12.0F};
    const simple_platformer::PlatformerMovementConfig config;
    const std::vector<simple_platformer::RouteConnection> connections =
        simple_platformer::buildPlatformerConnections(
            map,
            {2, 2},
            simple_platformer::PlatformerTraversalProfile{
                bodySize, config, tests::FixedStepSeconds})
            .connections;
    const simple_platformer::RouteConnection& jump =
        tests::connectionWith(connections, simple_platformer::Traversal::Jump);

    // The same jump, but its waypoint claims a row above where it really lands.
    simple_platformer::NavigationPath path = tests::floorPath({2, 2}, {jump.step});
    path.waypoints.front().feet.y -= static_cast<float>(tests::TileSize);
    simple_platformer::PathFollower follower;
    simple_platformer::setPath(follower, path);
    simple_platformer::Body body{
        simple_platformer::boxInCell(tests::TileSize, {2, 2}, bodySize), {0.0F, 0.0F}};
    simple_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};

    for (int tick = 0; tick < 240 && follower.path.has_value(); ++tick)
    {
        const simple_platformer::InputIntentions intentions =
            simple_platformer::followPlatformerPath(
                body, movement, follower, tests::FixedStepSeconds);
        simple_platformer::updatePlatformerMovement(
            map, body, movement, intentions, tests::FixedStepSeconds);
    }

    REQUIRE_FALSE(follower.path.has_value());
    REQUIRE(movement.grounded);
}

TEST_CASE("A jump replay stays started when no time has elapsed", "[navigation][follower]")
{
    simple_platformer::InputIntentions jump;
    jump.jumpPressed = true;
    jump.jumpHeld = true;
    simple_platformer::PathFollower follower;
    simple_platformer::setPath(
        follower,
        {{16.0F, 32.0F}, {{{48.0F, 32.0F}, simple_platformer::Traversal::Jump, {{0.1F, jump}}}}});
    simple_platformer::Body body{{{10.0F, 20.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    simple_platformer::PlatformerMovement movement;
    movement.grounded = true;

    const auto first = simple_platformer::followPlatformerPath(body, movement, follower, 0.0F);
    REQUIRE(first.jumpPressed);
    REQUIRE(follower.phase == simple_platformer::PathStepPhase::ReplayInputs);
    REQUIRE(follower.programElapsed == 0.0F);

    // Moving away from takeoff must not send an already-started replay back to approach.
    movement.grounded = false;
    body.bounds.topLeft.y -= 4.0F;
    const auto replay = simple_platformer::followPlatformerPath(body, movement, follower, 0.1F);
    REQUIRE(replay.jumpPressed);
    REQUIRE(follower.phase == simple_platformer::PathStepPhase::AwaitArrival);
    REQUIRE_FALSE(simple_platformer::pathComplete(follower));

    const auto waiting = simple_platformer::followPlatformerPath(body, movement, follower, 0.1F);
    REQUIRE_FALSE(waiting.jumpPressed);
    REQUIRE(follower.phase == simple_platformer::PathStepPhase::AwaitArrival);
    REQUIRE_FALSE(simple_platformer::pathComplete(follower));

    movement.grounded = true;
    simple_platformer::moveFeetTo(body.bounds, {48.0F, 32.0F});
    simple_platformer::followPlatformerPath(body, movement, follower, 0.1F);
    REQUIRE(simple_platformer::pathComplete(follower));
    REQUIRE(follower.phase == simple_platformer::PathStepPhase::ApproachStart);
    REQUIRE(follower.programElapsed == 0.0F);
}

TEST_CASE("Replacing or clearing a path resets its traversal phase", "[navigation][follower]")
{
    simple_platformer::PathFollower follower;
    follower.phase = simple_platformer::PathStepPhase::AwaitArrival;
    follower.programElapsed = 0.5F;
    simple_platformer::setPath(follower, {{16.0F, 32.0F}, {}});
    REQUIRE(follower.phase == simple_platformer::PathStepPhase::ApproachStart);
    REQUIRE(follower.programElapsed == 0.0F);

    follower.phase = simple_platformer::PathStepPhase::ReplayInputs;
    follower.programElapsed = 0.1F;
    simple_platformer::clearPath(follower);
    REQUIRE_FALSE(follower.path.has_value());
    REQUIRE(follower.phase == simple_platformer::PathStepPhase::ApproachStart);
    REQUIRE(follower.programElapsed == 0.0F);
}
