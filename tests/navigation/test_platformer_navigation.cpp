#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/connection_cache.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_search.hpp"
#include "simple_platformer/navigation/platformer_connections.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/fixed_step.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"

namespace
{
    using simple_platformer::NavigationConnection;
    using simple_platformer::Traversal;

    constexpr glm::vec2 SmallBody{12.0F, 12.0F};
    constexpr glm::vec2 TallBody{12.0F, 20.0F};

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

    bool hasStep(const simple_platformer::NavigationPath& route, Traversal traversal)
    {
        return std::any_of(
            route.steps.begin(),
            route.steps.end(),
            [traversal](const simple_platformer::NavigationStep& step)
            { return step.traversal == traversal; });
    }
}

TEST_CASE("The tick heuristic is an optimistic horizontal estimate", "[navigation][platformer]")
{
    const simple_platformer::PlatformerMovementConfig movement;
    // Nothing to travel across columns, however far down the goal is.
    REQUIRE(
        simple_platformer::platformerTickHeuristic(
            tests::TileSize, {2, 1}, {2, 8}, movement, tests::FixedStepSeconds) == 0);
    REQUIRE(
        simple_platformer::platformerTickHeuristic(
            tests::TileSize, {2, 1}, {3, 1}, movement, tests::FixedStepSeconds) == 5);

    simple_platformer::PlatformerMovementConfig slower = movement;
    slower.maximumSpeed = movement.maximumSpeed * 0.5F;
    REQUIRE(
        simple_platformer::platformerTickHeuristic(
            tests::TileSize, {2, 1}, {3, 1}, slower, tests::FixedStepSeconds) >
        simple_platformer::platformerTickHeuristic(
            tests::TileSize, {2, 1}, {3, 1}, movement, tests::FixedStepSeconds));

    simple_platformer::PlatformerMovementConfig invalid = movement;
    invalid.maximumSpeed = -1.0F;
    REQUIRE_THROWS_AS(
        simple_platformer::platformerTickHeuristic(
            tests::TileSize, {0, 0}, {1, 0}, invalid, tests::FixedStepSeconds),
        std::invalid_argument);
    invalid.maximumSpeed = std::numeric_limits<float>::infinity();
    REQUIRE_THROWS_AS(
        simple_platformer::platformerTickHeuristic(
            tests::TileSize, {0, 0}, {1, 0}, invalid, tests::FixedStepSeconds),
        std::invalid_argument);
}

TEST_CASE(
    "The jump start penalty stops a needless hop but not a needed jump",
    "[navigation][platformer][regression]")
{
    // A platform along the way that a hop over would save a few ticks.
    const simple_platformer::TileMap hop =
        tests::TileMapBuilder({".....###.....", ".............", ".............", "#############"});
    simple_platformer::PlatformerMovementConfig movement;
    movement.maximumSpeed = 60.0F;
    simple_platformer::PlatformerConnectionCache cache;
    const simple_platformer::NavigationPath preferred =
        simple_platformer::findPlatformerPath(
            hop, {12, 2}, {4, 2}, TallBody, movement, tests::FixedStepSeconds, cache)
            .path.value_or(simple_platformer::NavigationPath{});
    REQUIRE_FALSE(preferred.steps.empty());
    REQUIRE_FALSE(hasStep(preferred, Traversal::Jump));
    simple_platformer::PlatformerNavigationConfig noPenalty;
    noPenalty.jumpStartPenaltyTicks = 0;
    const simple_platformer::NavigationPath fastest =
        simple_platformer::findPlatformerPath(
            hop, {12, 2}, {4, 2}, TallBody, movement, tests::FixedStepSeconds, cache, noPenalty)
            .path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(hasStep(fastest, Traversal::Jump));

    // A goal on a platform is reached only by jumping, penalty or not.
    const simple_platformer::TileMap platform =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const simple_platformer::PlatformerMovementConfig ordinary;
    const simple_platformer::PlatformerTraversalProfile profile{
        SmallBody, ordinary, tests::FixedStepSeconds};
    const std::vector<NavigationConnection> connections =
        simple_platformer::buildPlatformerConnections(platform, {2, 2}, profile).connections;
    const NavigationConnection& up = jumpUpFrom(connections, 2);
    simple_platformer::PlatformerConnectionCache platformCache;
    const simple_platformer::NavigationPath climbed =
        simple_platformer::findPlatformerPath(
            platform,
            {2, 2},
            up.step.destinationCell,
            SmallBody,
            ordinary,
            tests::FixedStepSeconds,
            platformCache)
            .path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(hasStep(climbed, Traversal::Jump));
}

TEST_CASE("The tick heuristic uses the caller's step", "[navigation][platformer]")
{
    const simple_platformer::PlatformerMovementConfig movement;
    const int ticks = simple_platformer::platformerTickHeuristic(
        tests::TileSize, {2, 1}, {3, 1}, movement, tests::FixedStepSeconds);
    const int ticksAtDoubleStep = simple_platformer::platformerTickHeuristic(
        tests::TileSize, {2, 1}, {3, 1}, movement, tests::FixedStepSeconds * 2.0F);
    REQUIRE(ticks == 5);
    REQUIRE(ticksAtDoubleStep == 3);
}

TEST_CASE(
    "Platformer search rejects an invalid step or penalty",
    "[navigation][platformer][validation]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"...", "###"});
    const simple_platformer::PlatformerMovementConfig movement;
    simple_platformer::PlatformerConnectionCache cache;
    for (const float step :
         {0.0F, -tests::FixedStepSeconds, std::numeric_limits<float>::infinity()})
    {
        REQUIRE_THROWS_AS(
            simple_platformer::platformerTickHeuristic(
                tests::TileSize, {0, 0}, {1, 0}, movement, step),
            std::invalid_argument);
        REQUIRE_THROWS_AS(
            simple_platformer::findPlatformerPath(
                map, {0, 0}, {1, 0}, SmallBody, movement, step, cache),
            std::invalid_argument);
    }

    simple_platformer::PlatformerNavigationConfig negative;
    negative.jumpStartPenaltyTicks = -1;
    REQUIRE_THROWS_AS(
        simple_platformer::findPlatformerPath(
            map, {0, 0}, {1, 0}, SmallBody, movement, tests::FixedStepSeconds, cache, negative),
        std::invalid_argument);
}

TEST_CASE("Platformer searches report what they cost", "[navigation][platformer]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const simple_platformer::PlatformerMovementConfig movement;
    simple_platformer::PlatformerConnectionCache cache;

    const simple_platformer::PlatformerTraversalProfile profile{
        SmallBody, movement, tests::FixedStepSeconds};
    const simple_platformer::BuiltPlatformerConnections built =
        simple_platformer::buildPlatformerConnections(map, {2, 2}, profile);
    REQUIRE(!built.connections.empty());
    REQUIRE(built.simulatedTicks > 0);

    simple_platformer::PathSearchStatistics searchCost;
    const std::optional<simple_platformer::NavigationPath> path =
        simple_platformer::findPlatformerPath(
            map,
            {2, 2},
            {7, 2},
            SmallBody,
            movement,
            tests::FixedStepSeconds,
            cache,
            {},
            &searchCost)
            .path;
    REQUIRE(path.has_value());
    // The search expands at least its start cell, and simulating that cell's connections
    // is part of what it cost.
    REQUIRE(searchCost.nodesExpanded >= 1);
    REQUIRE(searchCost.simulatedTicks >= built.simulatedTicks);
}
