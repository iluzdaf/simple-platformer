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
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/fixed_step.hpp"
#include "support/neighbor_with.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"

namespace
{
    using simple_platformer::GridPosition;
    using simple_platformer::NavigationNeighbor;
    using simple_platformer::Traversal;

    constexpr glm::vec2 SmallBody{12.0F, 12.0F};

    bool hasConnection(
        const std::vector<NavigationNeighbor>& neighbors,
        GridPosition destination,
        Traversal traversal)
    {
        return std::any_of(
            neighbors.begin(),
            neighbors.end(),
            [destination, traversal](const NavigationNeighbor& neighbor)
            { return neighbor.destinationCell == destination && neighbor.traversal == traversal; });
    }

    const NavigationNeighbor& jumpUpFrom(const std::vector<NavigationNeighbor>& neighbors, int row)
    {
        const auto jump = std::find_if(
            neighbors.begin(),
            neighbors.end(),
            [row](const NavigationNeighbor& neighbor)
            { return neighbor.traversal == Traversal::Jump && neighbor.destinationCell.y < row; });
        if (jump == neighbors.end())
        {
            throw std::logic_error("No jump lands above the row");
        }
        return *jump;
    }
}

TEST_CASE(
    "A cell's connections are walks along its floor, a fall off its edge, and jumps",
    "[navigation][platformer]")
{
    const simple_platformer::PlatformerMovementConfig movement;

    // From the second cell of a floor of six: one walk to every other cell, and a direct
    // walk costs less than stopping at each cell on the way.
    const simple_platformer::TileMap floor = tests::TileMapBuilder({"......", "######"});
    const std::vector<NavigationNeighbor> walks = simple_platformer::platformerNeighbors(
        floor, {1, 0}, SmallBody, movement, tests::FixedStepSeconds);
    REQUIRE(
        std::count_if(
            walks.begin(),
            walks.end(),
            [](const NavigationNeighbor& neighbor)
            { return neighbor.traversal == Traversal::Walk; }) == 5);
    REQUIRE(hasConnection(walks, {0, 0}, Traversal::Walk));
    REQUIRE(hasConnection(walks, {5, 0}, Traversal::Walk));
    const NavigationNeighbor& direct = tests::neighborWith(walks, {3, 0}, Traversal::Walk);
    const NavigationNeighbor& first = tests::neighborWith(walks, {2, 0}, Traversal::Walk);
    const std::vector<NavigationNeighbor> onward = simple_platformer::platformerNeighbors(
        floor, {2, 0}, SmallBody, movement, tests::FixedStepSeconds);
    const NavigationNeighbor& second = tests::neighborWith(onward, {3, 0}, Traversal::Walk);
    REQUIRE(direct.cost < first.cost + second.cost);

    // From the edge of a ledge: a fall to the floor below, with the inputs to replay.
    const simple_platformer::TileMap ledge =
        tests::TileMapBuilder({"........", "###.....", "........", "........", "########"});
    const std::vector<NavigationNeighbor> offTheEdge = simple_platformer::platformerNeighbors(
        ledge, {2, 0}, SmallBody, movement, tests::FixedStepSeconds);
    const NavigationNeighbor& fall = tests::neighborWith(offTheEdge, Traversal::Fall);
    REQUIRE(fall.destinationCell.y > 0);
    REQUIRE_FALSE(fall.inputs.empty());

    // From the floor beside a platform: a jump up onto it.
    const simple_platformer::TileMap platform =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const std::vector<NavigationNeighbor> beside = simple_platformer::platformerNeighbors(
        platform, {2, 2}, SmallBody, movement, tests::FixedStepSeconds);
    const NavigationNeighbor& jump = jumpUpFrom(beside, 2);
    REQUIRE_FALSE(jump.inputs.empty());
}

TEST_CASE("A recorded jump replays to the landing it promised", "[navigation][platformer]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const simple_platformer::PlatformerMovementConfig config;
    const std::vector<NavigationNeighbor> neighbors = simple_platformer::platformerNeighbors(
        map, {2, 2}, SmallBody, config, tests::FixedStepSeconds);
    const NavigationNeighbor& jump = tests::neighborWith(neighbors, Traversal::Jump);

    simple_platformer::Body body{
        simple_platformer::boxInCell(tests::TileSize, {2, 2}, SmallBody), {0.0F, 0.0F}};
    simple_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};
    const float duration = simple_platformer::durationOf(jump.inputs);
    const long tickCount = std::lround(duration / tests::FixedStepSeconds);
    for (int tick = 0; tick < tickCount; ++tick)
    {
        const float elapsed = static_cast<float>(tick) * tests::FixedStepSeconds;
        const simple_platformer::InputIntentions intentions =
            simple_platformer::replayInput(jump.inputs, elapsed);
        simple_platformer::updatePlatformerMovement(
            map, body, movement, intentions, tests::FixedStepSeconds);
    }

    REQUIRE(movement.grounded);
    REQUIRE(
        simple_platformer::cellAtFeet(tests::TileSize, simple_platformer::feetOf(body.bounds)) ==
        jump.destinationCell);
}

TEST_CASE(
    "A connection's cost is the movement ticks it takes, never below the heuristic",
    "[navigation][platformer]")
{
    const simple_platformer::PlatformerMovementConfig config;

    // A walk costs as many ticks as following it takes.
    const simple_platformer::TileMap walkMap = tests::TileMapBuilder({"....", "####"});
    const std::vector<NavigationNeighbor> walkNeighbors = simple_platformer::platformerNeighbors(
        walkMap, {1, 0}, SmallBody, config, tests::FixedStepSeconds);
    const NavigationNeighbor& walk = tests::neighborWith(walkNeighbors, Traversal::Walk);
    simple_platformer::PathFollower follower;
    simple_platformer::setPath(
        follower, {{1, 0}, {{walk.destinationCell, walk.traversal, {}}}}, walk.destinationCell);
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
    REQUIRE(
        simple_platformer::platformerTickHeuristic(
            tests::TileSize, {1, 0}, walk.destinationCell, config, tests::FixedStepSeconds) <=
        walk.cost);

    // A jump or a fall costs as many ticks as its recorded inputs last.
    const simple_platformer::TileMap jumpMap =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const std::vector<NavigationNeighbor> jumpNeighbors = simple_platformer::platformerNeighbors(
        jumpMap, {2, 2}, SmallBody, config, tests::FixedStepSeconds);
    const NavigationNeighbor& jump = tests::neighborWith(jumpNeighbors, Traversal::Jump);
    REQUIRE(
        jump.cost ==
        std::lround(simple_platformer::durationOf(jump.inputs) / tests::FixedStepSeconds));
    REQUIRE(
        simple_platformer::platformerTickHeuristic(
            tests::TileSize, {2, 2}, jump.destinationCell, config, tests::FixedStepSeconds) <=
        jump.cost);

    const simple_platformer::TileMap fallMap =
        tests::TileMapBuilder({"........", "###.....", "........", "........", "########"});
    const std::vector<NavigationNeighbor> fallNeighbors = simple_platformer::platformerNeighbors(
        fallMap, {2, 0}, SmallBody, config, tests::FixedStepSeconds);
    const NavigationNeighbor& fall = tests::neighborWith(fallNeighbors, Traversal::Fall);
    REQUIRE(
        fall.cost ==
        std::lround(simple_platformer::durationOf(fall.inputs) / tests::FixedStepSeconds));
    REQUIRE(
        simple_platformer::platformerTickHeuristic(
            tests::TileSize, {2, 0}, fall.destinationCell, config, tests::FixedStepSeconds) <=
        fall.cost);
}

TEST_CASE("A recorded jump uses the caller's step", "[navigation][platformer]")
{
    const simple_platformer::PlatformerMovementConfig movement;
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const std::vector<NavigationNeighbor> neighbors = simple_platformer::platformerNeighbors(
        map, {2, 2}, SmallBody, movement, tests::FixedStepSeconds);
    const NavigationNeighbor& jump = tests::neighborWith(neighbors, Traversal::Jump);
    REQUIRE_THAT(
        simple_platformer::durationOf(jump.inputs),
        Catch::Matchers::WithinAbs(
            static_cast<float>(jump.cost) * tests::FixedStepSeconds, 0.001F));
}

TEST_CASE("Platformer connections reject an invalid step", "[navigation][platformer][validation]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"...", "###"});
    const simple_platformer::PlatformerMovementConfig movement;
    for (const float step :
         {0.0F, -tests::FixedStepSeconds, std::numeric_limits<float>::infinity()})
    {
        REQUIRE_THROWS_AS(
            simple_platformer::platformerNeighbors(map, {0, 0}, SmallBody, movement, step),
            std::invalid_argument);
    }
}
