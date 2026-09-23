#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/connection_cache.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_search.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/fixed_step.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    using simple_platformer::ConnectionBody;
    using simple_platformer::GridPosition;
    using simple_platformer::NavigationNeighbor;
    using simple_platformer::PathSearchStatistics;
    using simple_platformer::PlatformerConnectionCache;
    using simple_platformer::PlatformerMovementConfig;

    constexpr glm::vec2 BodySize{12.0F, 12.0F};

    void requireSameConnections(
        const std::vector<NavigationNeighbor>& left,
        const std::vector<NavigationNeighbor>& right)
    {
        REQUIRE(left.size() == right.size());
        for (std::size_t index = 0; index < left.size(); ++index)
        {
            REQUIRE(left[index].destinationCell == right[index].destinationCell);
            REQUIRE(left[index].traversal == right[index].traversal);
            REQUIRE(left[index].cost == right[index].cost);
            REQUIRE(left[index].inputs.size() == right[index].inputs.size());
        }
    }
}

TEST_CASE("Kept connections come back in place of simulating a cell again", "[navigation][cache]")
{
    // A ledge with a drop and a gap: walks, a fall and jumps leave the middle cell.
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "........", "###..###", "########"});
    const GridPosition cell{1, 2};
    PlatformerConnectionCache cache;
    PathSearchStatistics first;
    PathSearchStatistics second;

    const std::vector<NavigationNeighbor> simulated = simple_platformer::platformerNeighbors(
        map, cell, BodySize, {}, tests::FixedStepSeconds, &first, &cache);
    REQUIRE_FALSE(simulated.empty());
    REQUIRE(first.simulatedTicks > 0);
    REQUIRE(first.cellsReused == 0);
    REQUIRE(cache.size() == 1);

    const std::vector<NavigationNeighbor> kept = simple_platformer::platformerNeighbors(
        map, cell, BodySize, {}, tests::FixedStepSeconds, &second, &cache);
    REQUIRE(second.simulatedTicks == 0);
    REQUIRE(second.cellsReused == 1);
    REQUIRE(cache.size() == 1);
    requireSameConnections(kept, simulated);

    // Without a cache every call simulates, as before.
    PathSearchStatistics uncached;
    simple_platformer::platformerNeighbors(
        map, cell, BodySize, {}, tests::FixedStepSeconds, &uncached);
    REQUIRE(uncached.simulatedTicks == first.simulatedTicks);
}

TEST_CASE("Connections are kept apart for each body and step", "[navigation][cache]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "########"});
    const GridPosition cell{3, 1};
    PlatformerConnectionCache cache;
    const ConnectionBody body{BodySize, {}, tests::FixedStepSeconds};
    simple_platformer::platformerNeighbors(
        map, cell, BodySize, {}, tests::FixedStepSeconds, nullptr, &cache);
    REQUIRE(cache.find(cell, body) != nullptr);
    REQUIRE(cache.find({0, 1}, body) == nullptr);

    ConnectionBody taller = body;
    taller.size.y = 20.0F;
    REQUIRE(cache.find(cell, taller) == nullptr);
    ConnectionBody faster = body;
    faster.movement.maximumSpeed += 1.0F;
    REQUIRE(cache.find(cell, faster) == nullptr);
    ConnectionBody finer = body;
    finer.stepSeconds *= 0.5F;
    REQUIRE(cache.find(cell, finer) == nullptr);

    PathSearchStatistics statistics;
    simple_platformer::platformerNeighbors(
        map, cell, taller.size, taller.movement, taller.stepSeconds, &statistics, &cache);
    REQUIRE(statistics.cellsReused == 0);
    REQUIRE(statistics.simulatedTicks > 0);
    REQUIRE(cache.size() == 2);

    cache.clear();
    REQUIRE(cache.size() == 0);
    REQUIRE(cache.find(cell, body) == nullptr);
}

TEST_CASE(
    "A search through a cache finds the same path, then simulates nothing",
    "[navigation][cache]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "........", "###..###", "########"});
    const GridPosition start{0, 2};
    const GridPosition goal{7, 2};
    PathSearchStatistics uncached;
    const std::optional<simple_platformer::NavigationPath> expected =
        simple_platformer::findPlatformerPath(
            map, start, goal, BodySize, {}, tests::FixedStepSeconds, {}, &uncached);
    REQUIRE(expected.has_value());

    PlatformerConnectionCache cache;
    PathSearchStatistics filling;
    const std::optional<simple_platformer::NavigationPath> filled =
        simple_platformer::findPlatformerPath(
            map, start, goal, BodySize, {}, tests::FixedStepSeconds, {}, &filling, &cache);
    REQUIRE(filled.has_value());
    REQUIRE(filling.simulatedTicks == uncached.simulatedTicks);
    REQUIRE(filling.cellsReused == 0);

    PathSearchStatistics reusing;
    const std::optional<simple_platformer::NavigationPath> reused =
        simple_platformer::findPlatformerPath(
            map, start, goal, BodySize, {}, tests::FixedStepSeconds, {}, &reusing, &cache);
    REQUIRE(reused.has_value());
    REQUIRE(reusing.simulatedTicks == 0);
    REQUIRE(reusing.nodesExpanded == filling.nodesExpanded);
    REQUIRE(reusing.cellsReused == reusing.nodesExpanded);

    const std::vector<simple_platformer::NavigationStep> expectedSteps =
        expected.value_or(simple_platformer::NavigationPath{}).steps;
    const std::vector<simple_platformer::NavigationStep> reusedSteps =
        reused.value_or(simple_platformer::NavigationPath{}).steps;
    REQUIRE(reusedSteps.size() == expectedSteps.size());
    for (std::size_t index = 0; index < expectedSteps.size(); ++index)
    {
        REQUIRE(reusedSteps[index].destinationCell == expectedSteps[index].destinationCell);
        REQUIRE(reusedSteps[index].traversal == expectedSteps[index].traversal);
    }
}

TEST_CASE("A cell that cannot be stood on is kept as having no connections", "[navigation][cache]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "########"});
    PlatformerConnectionCache cache;
    PathSearchStatistics statistics;
    // In the air.
    REQUIRE(simple_platformer::platformerNeighbors(
                map, {3, 0}, BodySize, {}, tests::FixedStepSeconds, &statistics, &cache)
                .empty());
    REQUIRE(cache.size() == 1);
    simple_platformer::platformerNeighbors(
        map, {3, 0}, BodySize, {}, tests::FixedStepSeconds, &statistics, &cache);
    REQUIRE(statistics.cellsReused == 1);
}

TEST_CASE("Connections are kept only for a valid body", "[navigation][cache][validation]")
{
    PlatformerConnectionCache cache;
    ConnectionBody flat{{12.0F, 0.0F}, {}, tests::FixedStepSeconds};
    REQUIRE_THROWS_AS(cache.keep({0, 0}, flat, {}), std::invalid_argument);
    ConnectionBody stopped{BodySize, {}, 0.0F};
    REQUIRE_THROWS_AS(cache.keep({0, 0}, stopped, {}), std::invalid_argument);
    REQUIRE(cache.size() == 0);
}
