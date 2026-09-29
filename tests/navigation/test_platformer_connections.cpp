#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/input/input_program.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/navigation/route.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/platformer_cells.hpp"
#include "simple_platformer/navigation/platformer_connections.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/navigation/traversal.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/fixed_step.hpp"
#include "support/navigation_paths.hpp"
#include "support/route_connections.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"

namespace
{
    using simple_platformer::Cell;
    using simple_platformer::PlatformerTraversalProfile;
    using simple_platformer::RouteConnection;
    using simple_platformer::Traversal;

    constexpr glm::vec2 SmallBody{12.0F, 12.0F};

    bool hasConnection(
        const std::vector<RouteConnection>& connections,
        Cell destination,
        Traversal traversal)
    {
        return std::any_of(
            connections.begin(),
            connections.end(),
            [destination, traversal](const RouteConnection& connection)
            {
                return connection.step.destination.cell == destination &&
                       connection.step.traversal == traversal;
            });
    }
}

TEST_CASE("Walk connections reach every cell on the floor", "[navigation][platformer]")
{
    const PlatformerTraversalProfile profile{SmallBody, {}, tests::FixedStepSeconds};
    const simple_platformer::TileMap floor = tests::TileMapBuilder({"......", "######"});
    const std::vector<RouteConnection> walks =
        simple_platformer::buildPlatformerConnections(floor, {1, 0}, profile).connections;
    REQUIRE(
        std::count_if(
            walks.begin(),
            walks.end(),
            [](const RouteConnection& connection)
            { return connection.step.traversal == Traversal::Walk; }) == 5);
    REQUIRE(hasConnection(walks, {0, 0}, Traversal::Walk));
    REQUIRE(hasConnection(walks, {5, 0}, Traversal::Walk));
}

TEST_CASE("A direct walk costs less than stopping along the way", "[navigation][platformer]")
{
    const PlatformerTraversalProfile profile{SmallBody, {}, tests::FixedStepSeconds};
    const simple_platformer::TileMap floor = tests::TileMapBuilder({"......", "######"});
    const std::vector<RouteConnection> walks =
        simple_platformer::buildPlatformerConnections(floor, {1, 0}, profile).connections;
    const RouteConnection& direct = tests::connectionWith(walks, {3, 0}, Traversal::Walk);
    const RouteConnection& first = tests::connectionWith(walks, {2, 0}, Traversal::Walk);
    const std::vector<RouteConnection> onward =
        simple_platformer::buildPlatformerConnections(floor, {2, 0}, profile).connections;
    const RouteConnection& second = tests::connectionWith(onward, {3, 0}, Traversal::Walk);
    REQUIRE(direct.cost < first.cost + second.cost);
}

TEST_CASE("A fall from a ledge records inputs", "[navigation][platformer]")
{
    const simple_platformer::TileMap ledge =
        tests::TileMapBuilder({"........", "###.....", "........", "........", "########"});
    const PlatformerTraversalProfile profile{SmallBody, {}, tests::FixedStepSeconds};
    const std::vector<RouteConnection> offTheEdge =
        simple_platformer::buildPlatformerConnections(ledge, {2, 0}, profile).connections;
    const RouteConnection& fall = tests::connectionWith(offTheEdge, Traversal::Fall);
    REQUIRE(fall.step.destination.cell.y > 0);
    REQUIRE_FALSE(fall.step.inputs.empty());
}

TEST_CASE("A jump reaches the platform above and records inputs", "[navigation][platformer]")
{
    const simple_platformer::TileMap platform =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const PlatformerTraversalProfile profile{SmallBody, {}, tests::FixedStepSeconds};
    const std::vector<RouteConnection> beside =
        simple_platformer::buildPlatformerConnections(platform, {2, 2}, profile).connections;
    const RouteConnection& jump = tests::jumpUpFrom(beside, 2);
    REQUIRE_FALSE(jump.step.inputs.empty());
}

TEST_CASE("A recorded jump replays to the landing it promised", "[navigation][platformer]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const simple_platformer::PlatformerMovementConfig config;
    const PlatformerTraversalProfile profile{SmallBody, config, tests::FixedStepSeconds};
    const std::vector<RouteConnection> connections =
        simple_platformer::buildPlatformerConnections(map, {2, 2}, profile).connections;
    const RouteConnection& jump = tests::connectionWith(connections, Traversal::Jump);

    simple_platformer::Body body{
        simple_platformer::boxInCell(tests::TileSize, {2, 2}, SmallBody), {0.0F, 0.0F}};
    simple_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};
    const float duration = simple_platformer::durationOf(jump.step.inputs);
    const long tickCount = std::lround(duration / tests::FixedStepSeconds);
    for (int tick = 0; tick < tickCount; ++tick)
    {
        const float elapsed = static_cast<float>(tick) * tests::FixedStepSeconds;
        const simple_platformer::InputIntentions intentions =
            simple_platformer::replayInput(jump.step.inputs, elapsed);
        simple_platformer::updatePlatformerMovement(
            map, body, movement, intentions, tests::FixedStepSeconds);
    }

    REQUIRE(movement.grounded);
    REQUIRE(
        simple_platformer::cellAtFeet(tests::TileSize, simple_platformer::feetOf(body.bounds)) ==
        jump.step.destination.cell);
}

TEST_CASE("Failed airborne attempts still count their simulated ticks", "[navigation][platformer]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"##.##", "#####"});
    const PlatformerTraversalProfile profile{SmallBody, {}, tests::FixedStepSeconds};

    const simple_platformer::BuiltPlatformerConnections built =
        simple_platformer::buildPlatformerConnections(map, {2, 0}, profile);

    REQUIRE(built.connections.empty());
    REQUIRE(built.simulatedTicks > 0);
}

TEST_CASE("A walk connection costs the ticks its follower takes", "[navigation][platformer]")
{
    const simple_platformer::PlatformerMovementConfig config;
    const PlatformerTraversalProfile profile{SmallBody, config, tests::FixedStepSeconds};
    const simple_platformer::TileMap walkMap = tests::TileMapBuilder({"....", "####"});
    const std::vector<RouteConnection> walkConnections =
        simple_platformer::buildPlatformerConnections(walkMap, {1, 0}, profile).connections;
    const RouteConnection& walk = tests::connectionWith(walkConnections, Traversal::Walk);
    simple_platformer::PathFollower follower;
    simple_platformer::setPath(follower, tests::floorPath({1, 0}, {walk.step}));
    simple_platformer::Body body{
        simple_platformer::boxInCell(tests::TileSize, {1, 0}, SmallBody), {0.0F, 0.0F}};
    simple_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};
    int walkTicks = 0;
    while (walkTicks < 120 && !simple_platformer::pathComplete(follower))
    {
        const simple_platformer::InputIntentions intentions =
            simple_platformer::followPlatformerPath(
                body, movement, follower, tests::FixedStepSeconds);
        if (!simple_platformer::pathComplete(follower))
        {
            simple_platformer::updatePlatformerMovement(
                walkMap, body, movement, intentions, tests::FixedStepSeconds);
            ++walkTicks;
        }
    }
    REQUIRE(simple_platformer::pathComplete(follower));
    REQUIRE(walk.cost == walkTicks);
}

TEST_CASE("Airborne connection costs use the recorded program's ticks", "[navigation][platformer]")
{
    const PlatformerTraversalProfile profile{SmallBody, {}, tests::FixedStepSeconds};

    SECTION("jump")
    {
        const simple_platformer::TileMap map =
            tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
        const std::vector<RouteConnection> connections =
            simple_platformer::buildPlatformerConnections(map, {2, 2}, profile).connections;
        const RouteConnection& jump = tests::connectionWith(connections, Traversal::Jump);
        REQUIRE_THAT(
            simple_platformer::durationOf(jump.step.inputs),
            Catch::Matchers::WithinAbs(
                static_cast<float>(jump.cost) * profile.stepSeconds, 0.001F));
    }

    SECTION("fall")
    {
        const simple_platformer::TileMap map =
            tests::TileMapBuilder({"........", "###.....", "........", "........", "########"});
        const std::vector<RouteConnection> connections =
            simple_platformer::buildPlatformerConnections(map, {2, 0}, profile).connections;
        const RouteConnection& fall = tests::connectionWith(connections, Traversal::Fall);
        REQUIRE_THAT(
            simple_platformer::durationOf(fall.step.inputs),
            Catch::Matchers::WithinAbs(
                static_cast<float>(fall.cost) * profile.stepSeconds, 0.001F));
    }
}

TEST_CASE("Platformer connections reject an invalid step", "[navigation][platformer][validation]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"...", "###"});
    const simple_platformer::PlatformerMovementConfig movement;
    for (const float step :
         {0.0F, -tests::FixedStepSeconds, std::numeric_limits<float>::infinity()})
    {
        REQUIRE_THROWS_AS(
            simple_platformer::buildPlatformerConnections(
                map, {0, 0}, PlatformerTraversalProfile{SmallBody, movement, step}),
            std::invalid_argument);
    }
}

TEST_CASE(
    "A climber's cell holds the climbs leaving each surface",
    "[navigation][platformer][climb]")
{
    using simple_platformer::ClimbSurface;
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"......", ".c....", ".c....", ".c....", ".c....", "######"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    const PlatformerTraversalProfile walker{SmallBody, {}, tests::FixedStepSeconds};
    const PlatformerTraversalProfile climber{SmallBody, {}, tests::FixedStepSeconds, {{60.0F}}};
    const Cell besideWall{2, 4};

    const simple_platformer::BuiltPlatformerConnections walking =
        simple_platformer::buildPlatformerConnections(map, besideWall, walker);
    const simple_platformer::BuiltPlatformerConnections climbing =
        simple_platformer::buildPlatformerConnections(map, besideWall, climber);
    const auto climbs = [&climbing](ClimbSurface from, Cell cell, ClimbSurface surface)
    {
        return std::any_of(
            climbing.connections.begin(),
            climbing.connections.end(),
            [from, cell, surface](const RouteConnection& connection)
            {
                return connection.step.traversal == Traversal::Climb &&
                       connection.sourceSurface == from &&
                       connection.step.destination.cell == cell &&
                       connection.step.destination.surface == surface &&
                       !connection.step.inputs.empty() && connection.cost > 0;
            });
    };

    REQUIRE_FALSE(hasConnection(walking.connections, besideWall, Traversal::Climb));
    REQUIRE(climbs(ClimbSurface::None, besideWall, ClimbSurface::LeftWall));
    REQUIRE(climbs(ClimbSurface::LeftWall, besideWall, ClimbSurface::None));
    REQUIRE(climbs(ClimbSurface::LeftWall, {2, 3}, ClimbSurface::LeftWall));
    // No wall stands to the right, and the floor lies below.
    REQUIRE_FALSE(climbs(ClimbSurface::None, besideWall, ClimbSurface::RightWall));
    REQUIRE_FALSE(climbs(ClimbSurface::LeftWall, {2, 5}, ClimbSurface::LeftWall));
    // The walks, falls, and jumps from the floor are the walker's.
    REQUIRE(hasConnection(climbing.connections, {3, 4}, Traversal::Walk));
    REQUIRE(climbing.simulatedTicks > walking.simulatedTicks);
    REQUIRE(simple_platformer::contains(climbing.footprint, {1, 3}));

    // A cell in the air beside the wall holds climbs though nothing can stand in it.
    const simple_platformer::BuiltPlatformerConnections upTheWall =
        simple_platformer::buildPlatformerConnections(map, {2, 2}, climber);
    REQUIRE_FALSE(upTheWall.connections.empty());
    REQUIRE(std::all_of(
        upTheWall.connections.begin(),
        upTheWall.connections.end(),
        [](const RouteConnection& connection)
        {
            return connection.step.traversal == Traversal::Climb &&
                   connection.sourceSurface == ClimbSurface::LeftWall;
        }));
    REQUIRE(simple_platformer::buildPlatformerConnections(map, {2, 2}, walker).connections.empty());
}

TEST_CASE("A climber cannot hold an unmarked wall", "[navigation][platformer][climb]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"......", ".#....", ".#....", ".#....", ".#....", "######"});
    const PlatformerTraversalProfile climber{SmallBody, {}, tests::FixedStepSeconds, {{60.0F}}};

    const std::vector<RouteConnection> connections =
        simple_platformer::buildPlatformerConnections(map, {2, 4}, climber).connections;
    REQUIRE(hasConnection(connections, {3, 4}, Traversal::Walk));
    REQUIRE(std::none_of(
        connections.begin(),
        connections.end(),
        [](const RouteConnection& connection)
        { return connection.step.traversal == Traversal::Climb; }));
}

TEST_CASE(
    "A climb may reach a wall location that extends above the open map top",
    "[navigation][platformer][climb]")
{
    using simple_platformer::ClimbSurface;
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({".c..", ".c..", "####"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    const PlatformerTraversalProfile tallClimber{
        {12.0F, 40.0F}, {}, tests::FixedStepSeconds, {{60.0F}}};

    const std::vector<RouteConnection> connections =
        simple_platformer::buildPlatformerConnections(map, {2, 1}, tallClimber).connections;
    REQUIRE(std::any_of(
        connections.begin(),
        connections.end(),
        [](const RouteConnection& connection)
        {
            return connection.step.traversal == Traversal::Climb &&
                   connection.sourceSurface == ClimbSurface::LeftWall &&
                   connection.step.destination.cell == Cell{2, 0} &&
                   connection.step.destination.surface == ClimbSurface::LeftWall;
        }));
}
