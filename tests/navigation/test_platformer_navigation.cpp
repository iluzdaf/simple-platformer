#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

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
    constexpr glm::vec2 TallBody{12.0F, 20.0F};

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

    // The jump that lands above the row it leaves from, or a failure if none does.
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

    bool hasStep(const simple_platformer::NavigationPath& route, Traversal traversal)
    {
        return std::any_of(
            route.steps.begin(),
            route.steps.end(),
            [traversal](const simple_platformer::NavigationStep& step)
            { return step.traversal == traversal; });
    }
}

TEST_CASE("A standable cell has support below and room for the body", "[navigation][platformer]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".#.", "...", "###"});

    REQUIRE(simple_platformer::canStandAt(map, {1, 1}, SmallBody));
    // The tall body would reach into the tile above.
    REQUIRE_FALSE(simple_platformer::canStandAt(map, {1, 1}, TallBody));
    // Nothing below.
    REQUIRE_FALSE(simple_platformer::canStandAt(map, {1, 0}, SmallBody));
}

TEST_CASE(
    "A platformer start cell is the standable cell that supports the body",
    "[navigation][platformer]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "..###..."});

    // Standing on the platform, the cell under the feet.
    REQUIRE(
        simple_platformer::findPlatformerStartCell(
            map, simple_platformer::boxInCell(tests::TileSize, {3, 1}, TallBody)) ==
        GridPosition{3, 1});
    // A body wider than a tile is still placed by its feet.
    REQUIRE(
        simple_platformer::findPlatformerStartCell(
            map, simple_platformer::boxInCell(tests::TileSize, {3, 1}, {20.0F, 20.0F})) ==
        GridPosition{3, 1});

    // At the ledge the feet hang past the platform, so the cell under them cannot be
    // stood on; the start is the supporting cell the collider still rests on.
    simple_platformer::Aabb hanging{{0.0F, 0.0F}, TallBody};
    simple_platformer::placeFeetAt(hanging, {80.5F, 32.0F});
    REQUIRE(
        simple_platformer::cellAtFeet(tests::TileSize, simple_platformer::feetOf(hanging)) ==
        GridPosition{5, 1});
    REQUIRE(simple_platformer::findPlatformerStartCell(map, hanging) == GridPosition{4, 1});

    // In the air there is no start cell.
    REQUIRE(
        simple_platformer::findPlatformerStartCell(
            map, simple_platformer::boxInCell(tests::TileSize, {0, 0}, TallBody)) == std::nullopt);

    REQUIRE_THROWS_AS(
        simple_platformer::findPlatformerStartCell(map, {{0.0F, 0.0F}, {0.0F, 20.0F}}),
        std::invalid_argument);
}

TEST_CASE(
    "A chase cell is the target's cell when standable, else the nearest standable cell",
    "[navigation][platformer]")
{
    const simple_platformer::TileMap platform =
        tests::TileMapBuilder({"........", "........", "..###...", "........", "########"});
    // Standing on the platform, its own cell.
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(platform, {47.5F, 32.0F}, TallBody) ==
        GridPosition{2, 1});
    // Past either edge, or in the air above it, the nearest cell of the platform.
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(platform, {31.5F, 32.0F}, TallBody) ==
        GridPosition{2, 1});
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(platform, {80.5F, 32.0F}, TallBody) ==
        GridPosition{4, 1});
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(platform, {31.0F, 20.0F}, TallBody) ==
        GridPosition{2, 1});

    // The pursuer's own body decides what is standable: the tall body cannot fit under
    // the ceiling, and equal distances keep row, then column order, so the cell on the
    // left wins the tie.
    const simple_platformer::TileMap ceiling =
        tests::TileMapBuilder({"..#....", ".......", "#######"});
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(ceiling, {40.0F, 32.0F}, SmallBody) ==
        GridPosition{2, 1});
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(ceiling, {40.0F, 32.0F}, TallBody) ==
        GridPosition{1, 1});
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(ceiling, {8.0F, 32.0F}, {20.0F, 12.0F}) ==
        GridPosition{1, 1});

    // Feet off the map still find the nearest cell; a map with nowhere to stand has none.
    REQUIRE(
        simple_platformer::findPlatformerChaseCell(ceiling, {-16.0F, 32.0F}, TallBody) ==
        GridPosition{0, 1});
    const simple_platformer::TileMap solid = tests::TileMapBuilder({"###", "###"});
    REQUIRE_FALSE(simple_platformer::findPlatformerChaseCell(solid, {24.0F, 16.0F}, TallBody));
}

TEST_CASE("Chase cells match a scan of the whole map from anywhere", "[navigation][platformer]")
{
    // A standable target cell is kept; otherwise the closest standable cell by feet
    // distance, where equal distances keep row, then column order. This is the scan the
    // nearest-first search replaced, kept as its oracle.
    const auto closestByScan = [](const simple_platformer::TileMap& map,
                                  glm::vec2 feet,
                                  glm::vec2 bodySize) -> std::optional<GridPosition>
    {
        if (feet.x >= 0.0F && feet.x < map.pixelWidth() && feet.y >= 0.0F &&
            feet.y <= map.pixelHeight())
        {
            const GridPosition targetCell = simple_platformer::cellAtFeet(map.tileSize(), feet);
            if (simple_platformer::canStandAt(map, targetCell, bodySize))
            {
                return targetCell;
            }
        }
        std::optional<GridPosition> closest;
        double closestDistanceSquared = 0.0;
        for (int row = 0; row < map.height(); ++row)
        {
            for (int column = 0; column < map.width(); ++column)
            {
                const GridPosition candidate{column, row};
                if (!simple_platformer::canStandAt(map, candidate, bodySize))
                {
                    continue;
                }
                const glm::vec2 candidateFeet =
                    simple_platformer::feetInCell(map.tileSize(), candidate);
                const double dx = static_cast<double>(candidateFeet.x) - feet.x;
                const double dy = static_cast<double>(candidateFeet.y) - feet.y;
                const double distanceSquared = dx * dx + dy * dy;
                if (!closest.has_value() || distanceSquared < closestDistanceSquared)
                {
                    closest = candidate;
                    closestDistanceSquared = distanceSquared;
                }
            }
        }
        return closest;
    };

    const std::vector<simple_platformer::TileMap> maps = {
        tests::TileMapBuilder({"........", "........", "..###...", "........", "########"}),
        tests::TileMapBuilder({"..#....", ".......", "#######"}),
        tests::TileMapBuilder(
            {"##########", "#........#", "#..##..#.#", "#......#.#", "##########"}),
        tests::TileMapBuilder({"...", "...", "..."}),
    };
    const std::vector<glm::vec2> bodies = {SmallBody, TallBody, {20.0F, 12.0F}};
    for (const simple_platformer::TileMap& map : maps)
    {
        // Sample feet on a fine grid over the map and a margin outside it.
        const int height = static_cast<int>(map.pixelHeight());
        const int width = static_cast<int>(map.pixelWidth());
        for (int y = -24; y <= height + 24; y += 5)
        {
            for (int x = -24; x <= width + 24; x += 5)
            {
                const glm::vec2 feet{static_cast<float>(x), static_cast<float>(y)};
                for (const glm::vec2 body : bodies)
                {
                    REQUIRE(
                        simple_platformer::findPlatformerChaseCell(map, feet, body) ==
                        closestByScan(map, feet, body));
                }
            }
        }
    }
}

TEST_CASE("Chase cells reject invalid feet and bodies", "[navigation][platformer][validation]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    REQUIRE_THROWS_AS(
        simple_platformer::findPlatformerChaseCell(map, {infinity, 32.0F}, TallBody),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findPlatformerChaseCell(map, {24.0F, nan}, TallBody),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findPlatformerChaseCell(map, {24.0F, 32.0F}, {0.0F, 20.0F}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findPlatformerChaseCell(map, {24.0F, 32.0F}, {12.0F, -1.0F}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::findPlatformerChaseCell(map, {24.0F, 32.0F}, {infinity, 20.0F}),
        std::invalid_argument);
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

TEST_CASE(
    "A platformer search finds the same route with and without its heuristic",
    "[navigation][platformer]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".....", "#####"});
    const simple_platformer::PlatformerMovementConfig movement;

    const std::optional<simple_platformer::NavigationPath> path =
        simple_platformer::findPlatformerPath(
            map, {1, 0}, {3, 0}, SmallBody, movement, tests::FixedStepSeconds);
    REQUIRE(path.has_value());
    const simple_platformer::NavigationPath route =
        path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(route.start == GridPosition{1, 0});
    REQUIRE(route.steps.size() == 1);
    REQUIRE(route.steps.back().destinationCell == GridPosition{3, 0});
    REQUIRE(route.steps.back().traversal == Traversal::Walk);

    const simple_platformer::GridNeighborFunction neighbors =
        [&map,
         &movement](GridPosition position, const simple_platformer::GridNeighborVisitor& visit)
    {
        for (const NavigationNeighbor& neighbor : simple_platformer::platformerNeighbors(
                 map, position, SmallBody, movement, tests::FixedStepSeconds))
        {
            visit(neighbor, neighbor.cost);
        }
    };
    const std::optional<simple_platformer::NavigationPath> pathWithoutHeuristic =
        simple_platformer::findLowestCostPath({1, 0}, {3, 0}, map.size(), neighbors);
    REQUIRE(pathWithoutHeuristic.has_value());
    const simple_platformer::NavigationPath routeWithoutHeuristic =
        pathWithoutHeuristic.value_or(simple_platformer::NavigationPath{});
    REQUIRE(routeWithoutHeuristic.steps.size() == route.steps.size());
    for (std::size_t index = 0; index < route.steps.size(); ++index)
    {
        REQUIRE(
            routeWithoutHeuristic.steps[index].destinationCell ==
            route.steps[index].destinationCell);
        REQUIRE(routeWithoutHeuristic.steps[index].traversal == route.steps[index].traversal);
    }
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
    const simple_platformer::NavigationPath preferred =
        simple_platformer::findPlatformerPath(
            hop, {12, 2}, {4, 2}, TallBody, movement, tests::FixedStepSeconds)
            .value_or(simple_platformer::NavigationPath{});
    REQUIRE_FALSE(preferred.steps.empty());
    REQUIRE_FALSE(hasStep(preferred, Traversal::Jump));
    simple_platformer::PlatformerNavigationConfig noPenalty;
    noPenalty.jumpStartPenaltyTicks = 0;
    const simple_platformer::NavigationPath fastest =
        simple_platformer::findPlatformerPath(
            hop, {12, 2}, {4, 2}, TallBody, movement, tests::FixedStepSeconds, noPenalty)
            .value_or(simple_platformer::NavigationPath{});
    REQUIRE(hasStep(fastest, Traversal::Jump));

    // A goal on a platform is reached only by jumping, penalty or not.
    const simple_platformer::TileMap platform =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const simple_platformer::PlatformerMovementConfig ordinary;
    const std::vector<NavigationNeighbor> neighbors = simple_platformer::platformerNeighbors(
        platform, {2, 2}, SmallBody, ordinary, tests::FixedStepSeconds);
    const NavigationNeighbor& up = jumpUpFrom(neighbors, 2);
    const simple_platformer::NavigationPath climbed =
        simple_platformer::findPlatformerPath(
            platform, {2, 2}, up.destinationCell, SmallBody, ordinary, tests::FixedStepSeconds)
            .value_or(simple_platformer::NavigationPath{});
    REQUIRE(hasStep(climbed, Traversal::Jump));
}

TEST_CASE("Platformer navigation simulates at the step it is given", "[navigation][platformer]")
{
    const simple_platformer::PlatformerMovementConfig movement;
    // Twice the step covers the same distance in half the ticks.
    const int ticks = simple_platformer::platformerTickHeuristic(
        tests::TileSize, {2, 1}, {3, 1}, movement, tests::FixedStepSeconds);
    const int ticksAtDoubleStep = simple_platformer::platformerTickHeuristic(
        tests::TileSize, {2, 1}, {3, 1}, movement, tests::FixedStepSeconds * 2.0F);
    REQUIRE(ticks == 5);
    REQUIRE(ticksAtDoubleStep == 3);

    // A recorded jump lasts as many steps as it took to simulate.
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

TEST_CASE(
    "Platformer navigation rejects an invalid step or penalty",
    "[navigation][platformer][validation]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"...", "###"});
    const simple_platformer::PlatformerMovementConfig movement;
    for (const float step :
         {0.0F, -tests::FixedStepSeconds, std::numeric_limits<float>::infinity()})
    {
        REQUIRE_THROWS_AS(
            simple_platformer::platformerTickHeuristic(
                tests::TileSize, {0, 0}, {1, 0}, movement, step),
            std::invalid_argument);
        REQUIRE_THROWS_AS(
            simple_platformer::platformerNeighbors(map, {0, 0}, SmallBody, movement, step),
            std::invalid_argument);
        REQUIRE_THROWS_AS(
            simple_platformer::findPlatformerPath(map, {0, 0}, {1, 0}, SmallBody, movement, step),
            std::invalid_argument);
    }

    simple_platformer::PlatformerNavigationConfig negative;
    negative.jumpStartPenaltyTicks = -1;
    REQUIRE_THROWS_AS(
        simple_platformer::findPlatformerPath(
            map, {0, 0}, {1, 0}, SmallBody, movement, tests::FixedStepSeconds, negative),
        std::invalid_argument);
}

TEST_CASE("Platformer searches report what they cost", "[navigation][platformer]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const simple_platformer::PlatformerMovementConfig movement;

    simple_platformer::PathSearchStatistics neighborCost;
    const std::vector<NavigationNeighbor> neighbors = simple_platformer::platformerNeighbors(
        map, {2, 2}, SmallBody, movement, tests::FixedStepSeconds, &neighborCost);
    REQUIRE(!neighbors.empty());
    REQUIRE(neighborCost.nodesExpanded == 0);
    REQUIRE(neighborCost.simulatedTicks > 0);

    simple_platformer::PathSearchStatistics searchCost;
    const std::optional<simple_platformer::NavigationPath> path =
        simple_platformer::findPlatformerPath(
            map, {2, 2}, {7, 2}, SmallBody, movement, tests::FixedStepSeconds, {}, &searchCost);
    REQUIRE(path.has_value());
    // The search expands at least its start cell, and simulating that cell's connections
    // is part of what it cost.
    REQUIRE(searchCost.nodesExpanded >= 1);
    REQUIRE(searchCost.simulatedTicks >= neighborCost.simulatedTicks);
}
