#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/input_program.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
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
            { return neighbor.traversal == simple_platformer::Traversal::Walk; }) == 2);

    const simple_platformer::TileMap ledge = simple_platformer::TileMap::fromAscii(
        {"........", "###.....", "........", "........", "########"});
    const auto falls = simple_platformer::platformerNeighbors(
        ledge, {2, 0}, {12.0F, 12.0F}, simple_platformer::PlatformerMovementConfig{});
    const simple_platformer::NavigationNeighbor& fall =
        neighborWith(falls, simple_platformer::Traversal::Fall);
    REQUIRE(fall.destination.y > 0);
    REQUIRE_FALSE(fall.inputs.empty());
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
    "Airborne connection costs equal their input program durations in fixed ticks",
    "[navigation][platformer]")
{
    const simple_platformer::PlatformerMovementConfig config;
    constexpr float FixedDelta = static_cast<float>(simple_platformer::FixedDeltaSeconds);

    const simple_platformer::TileMap jumpMap = simple_platformer::TileMap::fromAscii(
        {"..........", "....##....", "..........", "##########"});
    const auto jumpNeighbors =
        simple_platformer::platformerNeighbors(jumpMap, {2, 2}, {12.0F, 12.0F}, config);
    const simple_platformer::NavigationNeighbor& jump =
        neighborWith(jumpNeighbors, simple_platformer::Traversal::Jump);
    REQUIRE(jump.cost == std::lround(simple_platformer::durationOf(jump.inputs) / FixedDelta));

    const simple_platformer::TileMap fallMap = simple_platformer::TileMap::fromAscii(
        {"........", "###.....", "........", "........", "########"});
    const auto fallNeighbors =
        simple_platformer::platformerNeighbors(fallMap, {2, 0}, {12.0F, 12.0F}, config);
    const simple_platformer::NavigationNeighbor& fall =
        neighborWith(fallNeighbors, simple_platformer::Traversal::Fall);
    REQUIRE(fall.cost == std::lround(simple_platformer::durationOf(fall.inputs) / FixedDelta));
}
