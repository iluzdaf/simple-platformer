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
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/platformer_connections.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/fixed_step.hpp"
#include "support/connection_with.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"

namespace
{
    using simple_platformer::GridPosition;
    using simple_platformer::NavigationConnection;
    using simple_platformer::PlatformerTraversalProfile;
    using simple_platformer::Traversal;

    constexpr glm::vec2 SmallBody{12.0F, 12.0F};

    bool hasConnection(
        const std::vector<NavigationConnection>& connections,
        GridPosition destination,
        Traversal traversal)
    {
        return std::any_of(
            connections.begin(),
            connections.end(),
            [destination, traversal](const NavigationConnection& connection)
            {
                return connection.step.destinationCell == destination &&
                       connection.step.traversal == traversal;
            });
    }

    const NavigationConnection& jumpUpFrom(
        const std::vector<NavigationConnection>& connections,
        int row)
    {
        const auto jump = std::find_if(
            connections.begin(),
            connections.end(),
            [row](const NavigationConnection& connection)
            {
                return connection.step.traversal == Traversal::Jump &&
                       connection.step.destinationCell.y < row;
            });
        if (jump == connections.end())
        {
            throw std::logic_error("No jump lands above the row");
        }
        return *jump;
    }

    void requireHeuristicBound(
        const simple_platformer::TileMap& map,
        GridPosition start,
        const PlatformerTraversalProfile& profile)
    {
        const std::vector<NavigationConnection> connections =
            simple_platformer::buildPlatformerConnections(map, start, profile).connections;
        REQUIRE_FALSE(connections.empty());
        for (const NavigationConnection& connection : connections)
        {
            CAPTURE(
                start.x,
                start.y,
                connection.step.destinationCell.x,
                connection.step.destinationCell.y);
            REQUIRE(
                simple_platformer::platformerTickHeuristic(
                    map.tileSize(),
                    start,
                    connection.step.destinationCell,
                    profile.movement,
                    profile.stepSeconds) <= connection.cost);
        }
    }
}

TEST_CASE("Walk connections reach every cell on the floor", "[navigation][platformer]")
{
    const PlatformerTraversalProfile profile{SmallBody, {}, tests::FixedStepSeconds};
    const simple_platformer::TileMap floor = tests::TileMapBuilder({"......", "######"});
    const std::vector<NavigationConnection> walks =
        simple_platformer::buildPlatformerConnections(floor, {1, 0}, profile).connections;
    REQUIRE(
        std::count_if(
            walks.begin(),
            walks.end(),
            [](const NavigationConnection& connection)
            { return connection.step.traversal == Traversal::Walk; }) == 5);
    REQUIRE(hasConnection(walks, {0, 0}, Traversal::Walk));
    REQUIRE(hasConnection(walks, {5, 0}, Traversal::Walk));
}

TEST_CASE("A direct walk costs less than stopping along the way", "[navigation][platformer]")
{
    const PlatformerTraversalProfile profile{SmallBody, {}, tests::FixedStepSeconds};
    const simple_platformer::TileMap floor = tests::TileMapBuilder({"......", "######"});
    const std::vector<NavigationConnection> walks =
        simple_platformer::buildPlatformerConnections(floor, {1, 0}, profile).connections;
    const NavigationConnection& direct = tests::connectionWith(walks, {3, 0}, Traversal::Walk);
    const NavigationConnection& first = tests::connectionWith(walks, {2, 0}, Traversal::Walk);
    const std::vector<NavigationConnection> onward =
        simple_platformer::buildPlatformerConnections(floor, {2, 0}, profile).connections;
    const NavigationConnection& second = tests::connectionWith(onward, {3, 0}, Traversal::Walk);
    REQUIRE(direct.cost < first.cost + second.cost);
}

TEST_CASE("A fall from a ledge records inputs", "[navigation][platformer]")
{
    const simple_platformer::TileMap ledge =
        tests::TileMapBuilder({"........", "###.....", "........", "........", "########"});
    const PlatformerTraversalProfile profile{SmallBody, {}, tests::FixedStepSeconds};
    const std::vector<NavigationConnection> offTheEdge =
        simple_platformer::buildPlatformerConnections(ledge, {2, 0}, profile).connections;
    const NavigationConnection& fall = tests::connectionWith(offTheEdge, Traversal::Fall);
    REQUIRE(fall.step.destinationCell.y > 0);
    REQUIRE_FALSE(fall.step.inputs.empty());
}

TEST_CASE("A jump reaches the platform above and records inputs", "[navigation][platformer]")
{
    const simple_platformer::TileMap platform =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const PlatformerTraversalProfile profile{SmallBody, {}, tests::FixedStepSeconds};
    const std::vector<NavigationConnection> beside =
        simple_platformer::buildPlatformerConnections(platform, {2, 2}, profile).connections;
    const NavigationConnection& jump = jumpUpFrom(beside, 2);
    REQUIRE_FALSE(jump.step.inputs.empty());
}

TEST_CASE("A recorded jump replays to the landing it promised", "[navigation][platformer]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const simple_platformer::PlatformerMovementConfig config;
    const PlatformerTraversalProfile profile{SmallBody, config, tests::FixedStepSeconds};
    const std::vector<NavigationConnection> connections =
        simple_platformer::buildPlatformerConnections(map, {2, 2}, profile).connections;
    const NavigationConnection& jump = tests::connectionWith(connections, Traversal::Jump);

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
        jump.step.destinationCell);
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
    const std::vector<NavigationConnection> walkConnections =
        simple_platformer::buildPlatformerConnections(walkMap, {1, 0}, profile).connections;
    const NavigationConnection& walk = tests::connectionWith(walkConnections, Traversal::Walk);
    simple_platformer::PathFollower follower;
    simple_platformer::setPath(follower, {{1, 0}, {walk.step}}, walk.step.destinationCell);
    simple_platformer::Body body{
        simple_platformer::boxInCell(tests::TileSize, {1, 0}, SmallBody), {0.0F, 0.0F}};
    simple_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};
    int walkTicks = 0;
    while (walkTicks < 120 && !simple_platformer::pathComplete(follower))
    {
        const simple_platformer::InputIntentions intentions =
            simple_platformer::followPlatformerPath(
                tests::TileSize, body, movement, follower, tests::FixedStepSeconds);
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
        const std::vector<NavigationConnection> connections =
            simple_platformer::buildPlatformerConnections(map, {2, 2}, profile).connections;
        const NavigationConnection& jump = tests::connectionWith(connections, Traversal::Jump);
        REQUIRE_THAT(
            simple_platformer::durationOf(jump.step.inputs),
            Catch::Matchers::WithinAbs(
                static_cast<float>(jump.cost) * profile.stepSeconds, 0.001F));
    }

    SECTION("fall")
    {
        const simple_platformer::TileMap map =
            tests::TileMapBuilder({"........", "###.....", "........", "........", "########"});
        const std::vector<NavigationConnection> connections =
            simple_platformer::buildPlatformerConnections(map, {2, 0}, profile).connections;
        const NavigationConnection& fall = tests::connectionWith(connections, Traversal::Fall);
        REQUIRE_THAT(
            simple_platformer::durationOf(fall.step.inputs),
            Catch::Matchers::WithinAbs(
                static_cast<float>(fall.cost) * profile.stepSeconds, 0.001F));
    }
}

TEST_CASE("The tick heuristic stays below simulated connection costs", "[navigation][platformer]")
{
    const PlatformerTraversalProfile profile{SmallBody, {}, tests::FixedStepSeconds};

    SECTION("flat floor")
    {
        requireHeuristicBound(tests::TileMapBuilder({"....", "####"}), {1, 0}, profile);
    }
    SECTION("ledge")
    {
        requireHeuristicBound(
            tests::TileMapBuilder({"........", "###.....", "........", "........", "########"}),
            {2, 0},
            profile);
    }
    SECTION("raised platform")
    {
        requireHeuristicBound(
            tests::TileMapBuilder({"..........", "....##....", "..........", "##########"}),
            {2, 2},
            profile);
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
