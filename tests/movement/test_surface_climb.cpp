#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/physics/body.hpp"
#include "support/require_near.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    using simple_platformer::Body;
    using simple_platformer::ClimbSurface;
    using simple_platformer::InputIntentions;
    using simple_platformer::PlatformerMovement;
    using simple_platformer::SurfaceClimb;
    using simple_platformer::WallHeading;

    const simple_platformer::TileMap Wall =
        tests::TileMapBuilder({"......", "..c...", "..c...", "..c...", "######"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
}

TEST_CASE("A climb request holds and moves along a wall", "[movement][climb]")
{
    Body body{{{48.0F, 36.0F}, {12.0F, 12.0F}}, {0.0F, 20.0F}};
    PlatformerMovement movement;
    SurfaceClimb climb{{60.0F}};
    InputIntentions intentions;
    intentions.climbRequested = true;
    intentions.direction.y = -1.0F;

    simple_platformer::updateSurfaceClimbMovement(Wall, body, movement, climb, intentions, 0.1F);

    REQUIRE(climb.surface == ClimbSurface::LeftWall);
    REQUIRE_NEAR(body.bounds.position.y, 30.0F);
    REQUIRE_NEAR(body.velocity.y, -60.0F);
    REQUIRE_FALSE(movement.grounded);
    REQUIRE(climb.wallHeading == WallHeading::Up);

    intentions.direction = {};
    simple_platformer::updateSurfaceClimbMovement(Wall, body, movement, climb, intentions, 0.1F);
    REQUIRE(climb.surface == ClimbSurface::LeftWall);
    REQUIRE_NEAR(body.bounds.position.y, 30.0F);
    REQUIRE_NEAR(body.velocity.y, 0.0F);

    intentions.direction.y = 1.0F;
    simple_platformer::updateSurfaceClimbMovement(Wall, body, movement, climb, intentions, 0.1F);
    REQUIRE_NEAR(body.bounds.position.y, 36.0F);
    REQUIRE(climb.wallHeading == WallHeading::Down);

    // Holding still keeps the way it last climbed.
    intentions.direction = {};
    simple_platformer::updateSurfaceClimbMovement(Wall, body, movement, climb, intentions, 0.1F);
    REQUIRE(climb.wallHeading == WallHeading::Down);
}

TEST_CASE("A wall heading follows the climb and resets off the wall", "[movement][climb]")
{
    InputIntentions still;
    InputIntentions up;
    up.direction.y = -1.0F;
    InputIntentions down;
    down.direction.y = 1.0F;

    for (const ClimbSurface wall : {ClimbSurface::LeftWall, ClimbSurface::RightWall})
    {
        REQUIRE(simple_platformer::wallHeadingFor(wall, up, WallHeading::Down) == WallHeading::Up);
        REQUIRE(
            simple_platformer::wallHeadingFor(wall, down, WallHeading::Up) == WallHeading::Down);
        REQUIRE(
            simple_platformer::wallHeadingFor(wall, still, WallHeading::Down) == WallHeading::Down);
    }
    for (const ClimbSurface offTheWall : {ClimbSurface::None, ClimbSurface::Ceiling})
    {
        REQUIRE(
            simple_platformer::wallHeadingFor(offTheWall, down, WallHeading::Down) ==
            WallHeading::Up);
    }
}

TEST_CASE("The opposite side of a wall can also be climbed", "[movement][climb]")
{
    Body body{{{20.0F, 36.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    PlatformerMovement movement;
    SurfaceClimb climb{{60.0F}};
    InputIntentions intentions;
    intentions.climbRequested = true;
    intentions.direction.y = -1.0F;

    simple_platformer::updateSurfaceClimbMovement(Wall, body, movement, climb, intentions, 0.1F);

    REQUIRE(climb.surface == ClimbSurface::RightWall);
    REQUIRE_NEAR(body.bounds.position.y, 30.0F);
}

TEST_CASE("A ceiling climb moves horizontally without gravity", "[movement][climb]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"......", ".ccc..", "......", "......", "######"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    Body body{{{32.0F, 32.0F}, {12.0F, 12.0F}}, {0.0F, 20.0F}};
    PlatformerMovement movement;
    SurfaceClimb climb{{60.0F}};
    InputIntentions intentions;
    intentions.climbRequested = true;
    intentions.direction.x = 1.0F;

    simple_platformer::updateSurfaceClimbMovement(map, body, movement, climb, intentions, 0.1F);

    REQUIRE(climb.surface == ClimbSurface::Ceiling);
    REQUIRE_NEAR(body.bounds.position.x, 38.0F);
    REQUIRE_NEAR(body.bounds.position.y, 32.0F);
    REQUIRE_NEAR(body.velocity.x, 60.0F);
    REQUIRE_NEAR(body.velocity.y, 0.0F);
}

TEST_CASE("A wall climber can turn onto a ceiling", "[movement][climb]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"......", "..ccc.", "..c...", "..c...", "######"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    Body body{{{48.0F, 36.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    PlatformerMovement movement;
    SurfaceClimb climb{{60.0F}};
    InputIntentions intentions;
    intentions.climbRequested = true;
    intentions.direction.y = -1.0F;
    simple_platformer::updateSurfaceClimbMovement(map, body, movement, climb, intentions, 0.1F);
    REQUIRE(climb.surface == ClimbSurface::LeftWall);
    REQUIRE_NEAR(body.bounds.position.y, 32.0F);

    intentions.direction = {1.0F, 0.0F};
    simple_platformer::updateSurfaceClimbMovement(map, body, movement, climb, intentions, 0.1F);

    REQUIRE(climb.surface == ClimbSurface::Ceiling);
    REQUIRE_NEAR(body.bounds.position.x, 54.0F);
    REQUIRE_NEAR(body.bounds.position.y, 32.0F);
}

TEST_CASE("Releasing climb resumes ordinary falling", "[movement][climb]")
{
    Body body{{{48.0F, 36.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    PlatformerMovement movement;
    SurfaceClimb climb{{60.0F}};
    InputIntentions intentions;
    intentions.climbRequested = true;
    simple_platformer::updateSurfaceClimbMovement(Wall, body, movement, climb, intentions, 0.1F);
    REQUIRE(climb.surface == ClimbSurface::LeftWall);

    intentions.climbRequested = false;
    simple_platformer::updateSurfaceClimbMovement(Wall, body, movement, climb, intentions, 0.1F);

    REQUIRE(climb.surface == ClimbSurface::None);
    REQUIRE(body.bounds.position.y > 36.0F);
    REQUIRE(body.velocity.y > 0.0F);
}

TEST_CASE("A climb request needs an adjacent surface", "[movement][climb]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"......", "......", "......", "......", "######"});
    Body body{{{48.0F, 36.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    PlatformerMovement movement;
    SurfaceClimb climb{{60.0F}};
    InputIntentions intentions;
    intentions.climbRequested = true;

    simple_platformer::updateSurfaceClimbMovement(map, body, movement, climb, intentions, 0.1F);

    REQUIRE(climb.surface == ClimbSurface::None);
    REQUIRE(body.velocity.y > 0.0F);
}

TEST_CASE("A climb request cannot attach to an unmarked solid wall", "[movement][climb]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"......", "..X...", "..X...", "..X...", "######"})
            .where('X', tests::Tile{}.blocksMovement());
    Body body{{{48.0F, 36.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    PlatformerMovement movement;
    SurfaceClimb climb{{60.0F}};
    InputIntentions intentions;
    intentions.climbRequested = true;
    intentions.direction.y = -1.0F;

    simple_platformer::updateSurfaceClimbMovement(map, body, movement, climb, intentions, 0.1F);

    REQUIRE(climb.surface == ClimbSurface::None);
    REQUIRE(body.velocity.y > 0.0F);
}

TEST_CASE("A climb request cannot attach to an unmarked solid ceiling", "[movement][climb]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"......", ".XXX..", "......", "......", "######"})
            .where('X', tests::Tile{}.blocksMovement());
    Body body{{{32.0F, 32.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    PlatformerMovement movement;
    SurfaceClimb climb{{60.0F}};
    InputIntentions intentions;
    intentions.climbRequested = true;
    intentions.direction.x = 1.0F;

    simple_platformer::updateSurfaceClimbMovement(map, body, movement, climb, intentions, 0.1F);

    REQUIRE(climb.surface == ClimbSurface::None);
    REQUIRE(body.velocity.y > 0.0F);
}

TEST_CASE("Climbing ends when the surface ends", "[movement][climb]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"......", ".cc...", "......", "......", "######"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    Body body{{{32.0F, 32.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    PlatformerMovement movement;
    SurfaceClimb climb{{60.0F}};
    InputIntentions intentions;
    intentions.climbRequested = true;
    intentions.direction.x = 1.0F;

    simple_platformer::updateSurfaceClimbMovement(map, body, movement, climb, intentions, 0.4F);

    REQUIRE(climb.surface == ClimbSurface::None);
    REQUIRE_NEAR(body.velocity.x, 0.0F);
    REQUIRE_NEAR(body.velocity.y, 0.0F);
    simple_platformer::updateSurfaceClimbMovement(map, body, movement, climb, intentions, 0.1F);
    REQUIRE(body.velocity.y > 0.0F);
}

TEST_CASE("Climbing requires a positive finite speed", "[movement][climb]")
{
    SurfaceClimb climb;
    climb.config.speed = 0.0F;
    REQUIRE_THROWS_AS(
        simple_platformer::validateSurfaceClimbConfig(climb.config), std::invalid_argument);
}
