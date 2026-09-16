#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/input_program.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/path_search.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/timing/fixed_step.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace
{
    const simple_platformer::NavigationNeighbor& neighborWith(
        const std::vector<simple_platformer::NavigationNeighbor>& neighbors,
        simple_platformer::Traversal traversal)
    {
        const auto neighbor = std::find_if(
            neighbors.begin(),
            neighbors.end(),
            [traversal](const simple_platformer::NavigationNeighbor& candidate)
            { return candidate.traversal == traversal; });
        if (neighbor == neighbors.end())
        {
            throw std::logic_error("The expected navigation neighbor was not generated");
        }
        return *neighbor;
    }
}

TEST_CASE("Standable cells require support and body clearance", "[navigation][platformer]")
{
    const simple_platformer::TileMap map =
        simple_platformer::TileMap::fromAscii({".#.", "...", "###"});

    REQUIRE(simple_platformer::canStandAt(map, {1, 1}, {12.0F, 12.0F}));
    REQUIRE_FALSE(simple_platformer::canStandAt(map, {1, 1}, {12.0F, 20.0F}));
    REQUIRE_FALSE(simple_platformer::canStandAt(map, {1, 0}, {12.0F, 12.0F}));
}

TEST_CASE(
    "Platformer tick heuristic is an optimistic horizontal estimate",
    "[navigation][platformer]")
{
    const simple_platformer::PlatformerMovementConfig movement;
    REQUIRE(simple_platformer::platformerTickHeuristic({2, 1}, {2, 8}, movement) == 0);
    REQUIRE(simple_platformer::platformerTickHeuristic({2, 1}, {3, 1}, movement) == 5);

    simple_platformer::PlatformerMovementConfig slower = movement;
    slower.maximumSpeed = movement.maximumSpeed * 0.5F;
    REQUIRE(
        simple_platformer::platformerTickHeuristic({2, 1}, {3, 1}, slower) >
        simple_platformer::platformerTickHeuristic({2, 1}, {3, 1}, movement));

    simple_platformer::PlatformerMovementConfig invalid = movement;
    invalid.maximumSpeed = -1.0F;
    REQUIRE_THROWS_AS(
        simple_platformer::platformerTickHeuristic({0, 0}, {1, 0}, invalid), std::invalid_argument);

    invalid.maximumSpeed = std::numeric_limits<float>::infinity();
    REQUIRE_THROWS_AS(
        simple_platformer::platformerTickHeuristic({0, 0}, {1, 0}, invalid), std::invalid_argument);
}

TEST_CASE("Platformer neighbors include walks and simulated falls", "[navigation][platformer]")
{
    const simple_platformer::TileMap floor =
        simple_platformer::TileMap::fromAscii({"....", "####"});
    const auto walks = simple_platformer::platformerNeighbors(
        floor, {1, 0}, {12.0F, 12.0F}, simple_platformer::PlatformerMovementConfig{});
    REQUIRE(
        std::count_if(
            walks.begin(),
            walks.end(),
            [](const simple_platformer::NavigationNeighbor& neighbor)
            { return neighbor.traversal == simple_platformer::Traversal::Walk; }) == 3);

    const simple_platformer::TileMap ledge = simple_platformer::TileMap::fromAscii(
        {"........", "###.....", "........", "........", "########"});
    const auto falls = simple_platformer::platformerNeighbors(
        ledge, {2, 0}, {12.0F, 12.0F}, simple_platformer::PlatformerMovementConfig{});
    const simple_platformer::NavigationNeighbor& fall =
        neighborWith(falls, simple_platformer::Traversal::Fall);
    REQUIRE(fall.destination.y > 0);
    REQUIRE_FALSE(fall.inputs.empty());
}

TEST_CASE("Platformer neighbors include continuous multi-cell walks", "[navigation][platformer]")
{
    const simple_platformer::TileMap map =
        simple_platformer::TileMap::fromAscii({"......", "######"});
    const simple_platformer::PlatformerMovementConfig movement;

    const auto neighbors =
        simple_platformer::platformerNeighbors(map, {1, 0}, {12.0F, 12.0F}, movement);
    const auto directWalk = std::find_if(
        neighbors.begin(),
        neighbors.end(),
        [](const simple_platformer::NavigationNeighbor& neighbor)
        {
            return neighbor.destination == simple_platformer::GridPosition{3, 0} &&
                   neighbor.traversal == simple_platformer::Traversal::Walk;
        });
    REQUIRE(directWalk != neighbors.end());

    const auto firstWalk = std::find_if(
        neighbors.begin(),
        neighbors.end(),
        [](const simple_platformer::NavigationNeighbor& neighbor)
        {
            return neighbor.destination == simple_platformer::GridPosition{2, 0} &&
                   neighbor.traversal == simple_platformer::Traversal::Walk;
        });
    REQUIRE(firstWalk != neighbors.end());
    const auto nextNeighbors = simple_platformer::platformerNeighbors(
        map, firstWalk->destination, {12.0F, 12.0F}, movement);
    const auto secondWalk = std::find_if(
        nextNeighbors.begin(),
        nextNeighbors.end(),
        [](const simple_platformer::NavigationNeighbor& neighbor)
        {
            return neighbor.destination == simple_platformer::GridPosition{3, 0} &&
                   neighbor.traversal == simple_platformer::Traversal::Walk;
        });
    REQUIRE(secondWalk != nextNeighbors.end());
    REQUIRE(directWalk->cost < firstWalk->cost + secondWalk->cost);
}

TEST_CASE("Generated jump inputs replay to their promised landing", "[navigation][platformer]")
{
    const simple_platformer::TileMap map = simple_platformer::TileMap::fromAscii(
        {"..........", "....##....", "..........", "##########"});
    const simple_platformer::PlatformerMovementConfig config;
    const auto neighbors =
        simple_platformer::platformerNeighbors(map, {2, 2}, {12.0F, 12.0F}, config);
    const simple_platformer::NavigationNeighbor& jump =
        neighborWith(neighbors, simple_platformer::Traversal::Jump);

    simple_platformer::Body body{{{0.0F, 0.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    simple_platformer::placeFeetAt(body.bounds, simple_platformer::navigationFeet({2, 2}));
    simple_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};
    simple_platformer::Facing facing = simple_platformer::Facing::Right;
    const float fixedDelta = static_cast<float>(simple_platformer::FixedDeltaSeconds);
    const float duration = simple_platformer::durationOf(jump.inputs);
    const long tickCount = std::lround(duration / fixedDelta);
    for (int tick = 0; tick < tickCount; ++tick)
    {
        const float elapsed = static_cast<float>(tick) * fixedDelta;
        const simple_platformer::InputIntentions intentions =
            simple_platformer::replayInput(jump.inputs, elapsed);
        simple_platformer::updatePlatformerMovement(
            map, body, movement, intentions, facing, fixedDelta);
    }

    REQUIRE(movement.grounded);
    REQUIRE(
        simple_platformer::navigationCell(simple_platformer::feetOf(body.bounds)) ==
        jump.destination);
}

TEST_CASE(
    "Platformer connection costs equal their fixed movement ticks",
    "[navigation][platformer]")
{
    const simple_platformer::PlatformerMovementConfig config;
    constexpr float FixedDelta = static_cast<float>(simple_platformer::FixedDeltaSeconds);

    const simple_platformer::TileMap walkMap =
        simple_platformer::TileMap::fromAscii({"....", "####"});
    const auto walkNeighbors =
        simple_platformer::platformerNeighbors(walkMap, {1, 0}, {12.0F, 12.0F}, config);
    const simple_platformer::NavigationNeighbor& walk =
        neighborWith(walkNeighbors, simple_platformer::Traversal::Walk);
    simple_platformer::PathFollower follower;
    simple_platformer::setPath(
        follower, {{1, 0}, {{walk.destination, walk.traversal, {}}}}, walk.destination);
    simple_platformer::Body body{{{0.0F, 0.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    simple_platformer::placeFeetAt(body.bounds, simple_platformer::navigationFeet({1, 0}));
    simple_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};
    simple_platformer::Facing facing = simple_platformer::Facing::Right;
    int walkTicks = 0;
    while (walkTicks < 120 && !simple_platformer::pathComplete(follower))
    {
        const simple_platformer::InputIntentions intentions =
            simple_platformer::followPlatformerPath(body, movement, follower, FixedDelta);
        if (!simple_platformer::pathComplete(follower))
        {
            simple_platformer::updatePlatformerMovement(
                walkMap, body, movement, intentions, facing, FixedDelta);
            ++walkTicks;
        }
    }
    REQUIRE(simple_platformer::pathComplete(follower));
    REQUIRE(walk.cost == walkTicks);
    REQUIRE(
        simple_platformer::platformerTickHeuristic({1, 0}, walk.destination, config) <= walk.cost);

    const simple_platformer::TileMap jumpMap = simple_platformer::TileMap::fromAscii(
        {"..........", "....##....", "..........", "##########"});
    const auto jumpNeighbors =
        simple_platformer::platformerNeighbors(jumpMap, {2, 2}, {12.0F, 12.0F}, config);
    const simple_platformer::NavigationNeighbor& jump =
        neighborWith(jumpNeighbors, simple_platformer::Traversal::Jump);
    REQUIRE(jump.cost == std::lround(simple_platformer::durationOf(jump.inputs) / FixedDelta));
    REQUIRE(
        simple_platformer::platformerTickHeuristic({2, 2}, jump.destination, config) <= jump.cost);

    const simple_platformer::TileMap fallMap = simple_platformer::TileMap::fromAscii(
        {"........", "###.....", "........", "........", "########"});
    const auto fallNeighbors =
        simple_platformer::platformerNeighbors(fallMap, {2, 0}, {12.0F, 12.0F}, config);
    const simple_platformer::NavigationNeighbor& fall =
        neighborWith(fallNeighbors, simple_platformer::Traversal::Fall);
    REQUIRE(fall.cost == std::lround(simple_platformer::durationOf(fall.inputs) / FixedDelta));
    REQUIRE(
        simple_platformer::platformerTickHeuristic({2, 0}, fall.destination, config) <= fall.cost);
}

TEST_CASE(
    "Platformer path search uses the platformer navigation policy",
    "[navigation][platformer]")
{
    const simple_platformer::TileMap map =
        simple_platformer::TileMap::fromAscii({".....", "#####"});

    const simple_platformer::PlatformerMovementConfig movement;
    const auto path =
        simple_platformer::findPlatformerPath(map, {1, 0}, {3, 0}, {12.0F, 12.0F}, movement);

    REQUIRE(path.has_value());
    const simple_platformer::NavigationPath route =
        path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(route.start == simple_platformer::GridPosition{1, 0});
    REQUIRE(route.steps.size() == 1);
    REQUIRE(route.steps.back().destination == simple_platformer::GridPosition{3, 0});
    REQUIRE(route.steps.back().traversal == simple_platformer::Traversal::Walk);

    const simple_platformer::GridNeighborFunction neighbors =
        [&map, &movement](simple_platformer::GridPosition position)
    { return simple_platformer::platformerNeighbors(map, position, {12.0F, 12.0F}, movement); };
    const auto pathWithoutHeuristic =
        simple_platformer::findLowestCostPath({1, 0}, {3, 0}, neighbors);
    REQUIRE(pathWithoutHeuristic.has_value());
    const simple_platformer::NavigationPath routeWithoutHeuristic =
        pathWithoutHeuristic.value_or(simple_platformer::NavigationPath{});
    REQUIRE(routeWithoutHeuristic.steps.size() == route.steps.size());
    for (std::size_t index = 0; index < route.steps.size(); ++index)
    {
        REQUIRE(routeWithoutHeuristic.steps[index].destination == route.steps[index].destination);
        REQUIRE(routeWithoutHeuristic.steps[index].traversal == route.steps[index].traversal);
    }
}
