#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/connection_cache.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_search.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/fixed_step.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    using simple_platformer::CellRange;
    using simple_platformer::ConnectionBody;
    using simple_platformer::GridPosition;
    using simple_platformer::NavigationNeighbor;
    using simple_platformer::PathSearchStatistics;
    using simple_platformer::PlatformerConnectionCache;
    using simple_platformer::RememberedWalk;

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

TEST_CASE("A cell is simulated once and read from the cache after", "[navigation][cache]")
{
    // A ledge with a drop and a gap: walks, a fall and jumps leave the middle cell.
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "........", "###..###", "########"});
    const GridPosition cell{1, 2};
    PlatformerConnectionCache cache;
    const ConnectionBody body{BodySize, {}, tests::FixedStepSeconds};

    PathSearchStatistics first;
    const std::vector<NavigationNeighbor> simulated = simple_platformer::platformerNeighbors(
        map, cell, BodySize, {}, tests::FixedStepSeconds, &first, &cache);
    REQUIRE_FALSE(simulated.empty());
    REQUIRE(first.simulatedTicks > 0);
    REQUIRE(first.cellsReused == 0);
    REQUIRE(cache.size() == 1);

    // Read again, by copy or in place, nothing is simulated and the cell counts as reused.
    PathSearchStatistics second;
    const std::vector<NavigationNeighbor> copied = simple_platformer::platformerNeighbors(
        map, cell, BodySize, {}, tests::FixedStepSeconds, &second, &cache);
    REQUIRE(second.simulatedTicks == 0);
    REQUIRE(second.cellsReused == 1);
    requireSameConnections(copied, simulated);
    PathSearchStatistics third;
    const std::vector<NavigationNeighbor>& inPlace = simple_platformer::platformerNeighborsKept(
        map, cell, BodySize, {}, tests::FixedStepSeconds, cache, &third);
    REQUIRE(third.simulatedTicks == 0);
    REQUIRE(third.cellsReused == 1);
    REQUIRE(&inPlace == cache.find(cell, body));
    REQUIRE(cache.size() == 1);

    // Without a cache every call simulates.
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
    // The cache lists the bodies it knows in the order first met.
    REQUIRE(cache.bodiesKept().size() == 2);
    REQUIRE(cache.bodiesKept()[0] == body);
    REQUIRE(cache.bodiesKept()[1] == taller);

    cache.clear();
    REQUIRE(cache.size() == 0);
    REQUIRE(cache.find(cell, body) == nullptr);
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

TEST_CASE("A walk is remembered per length and body, and a break leaves it", "[navigation][cache]")
{
    PlatformerConnectionCache cache;
    const ConnectionBody body{BodySize, {}, tests::FixedStepSeconds};
    const ConnectionBody taller{{12.0F, 20.0F}, {}, tests::FixedStepSeconds};
    REQUIRE(cache.walkKept(3, body) == nullptr);
    REQUIRE(cache.walksKept(body) == 0);

    cache.keepWalk(3, body, {35, {{-1, -1}, {4, 1}}});
    cache.keepWalk(-3, body, {36, {{-4, -1}, {1, 1}}});
    cache.keepWalk(12, body, {std::nullopt, {{-1, -1}, {13, 1}}});
    REQUIRE(cache.walksKept(body) == 3);
    REQUIRE(cache.walksKept(taller) == 0);
    REQUIRE(cache.walkKept(3, taller) == nullptr);
    const RememberedWalk* rightwards = cache.walkKept(3, body);
    REQUIRE(rightwards != nullptr);
    REQUIRE(rightwards->cost.value_or(0) == 35);
    REQUIRE(rightwards->sweep.last == GridPosition{4, 1});
    // Leftwards is its own length, and a walk past the limit is remembered as such.
    REQUIRE(cache.walkKept(-3, body)->cost.value_or(0) == 36);
    REQUIRE_FALSE(cache.walkKept(12, body)->cost.has_value());

    // Keeping again replaces; a break changes nothing, since no tile decided a walk.
    cache.keepWalk(3, body, {34, {{-1, -1}, {4, 1}}});
    REQUIRE(cache.walkKept(3, body)->cost.value_or(0) == 34);
    cache.keep({0, 1}, body, {}, {{-2, 0}, {6, 2}});
    cache.invalidate({2, 1});
    REQUIRE(cache.cellsKept(body) == 0);
    REQUIRE(cache.walksKept(body) == 3);

    cache.clear();
    REQUIRE(cache.walksKept(body) == 0);
}

TEST_CASE("Remembered walks change nothing but the ticks simulated", "[navigation][cache]")
{
    // A floor long enough that a walk along it runs past the simulation limit.
    const std::string open(40, '.');
    const std::string floor(40, '#');
    const simple_platformer::TileMap map = tests::TileMapBuilder({open, open, floor});
    PlatformerConnectionCache cache;
    const ConnectionBody body{BodySize, {}, tests::FixedStepSeconds};
    const GridPosition first{20, 1};
    const GridPosition second{25, 1};

    // The first cell simulates every length it can walk, and the one it cannot.
    PathSearchStatistics firstCost;
    simple_platformer::platformerNeighborsKept(
        map, first, BodySize, {}, tests::FixedStepSeconds, cache, &firstCost);
    const std::size_t walks = cache.walksKept(body);
    REQUIRE(walks > 0);
    bool pastTheLimit = false;
    for (int columns = -map.width(); columns <= map.width(); ++columns)
    {
        const RememberedWalk* walk = cache.walkKept(columns, body);
        pastTheLimit = pastTheLimit || (walk != nullptr && !walk->cost.has_value());
    }
    REQUIRE(pastTheLimit);

    // Another cell of the floor walks the same lengths, so it simulates only its jumps
    // and falls and remembers no new walk, yet its connections and footprint are the
    // ones it would have simulated alone.
    PathSearchStatistics secondCost;
    const std::vector<NavigationNeighbor>& remembered = simple_platformer::platformerNeighborsKept(
        map, second, BodySize, {}, tests::FixedStepSeconds, cache, &secondCost);
    REQUIRE(secondCost.simulatedTicks < firstCost.simulatedTicks);
    REQUIRE(cache.walksKept(body) == walks);
    PlatformerConnectionCache alone;
    const std::vector<NavigationNeighbor>& simulated = simple_platformer::platformerNeighborsKept(
        map, second, BodySize, {}, tests::FixedStepSeconds, alone);
    requireSameConnections(remembered, simulated);
    const CellRange rememberedFootprint = cache.footprintKept(second, body).value_or(CellRange{});
    const CellRange simulatedFootprint = alone.footprintKept(second, body).value_or(CellRange{});
    REQUIRE(rememberedFootprint.first == simulatedFootprint.first);
    REQUIRE(rememberedFootprint.last == simulatedFootprint.last);
    // Without a cache there is nothing to remember, so every walk is simulated.
    PathSearchStatistics aloneCost;
    simple_platformer::platformerNeighbors(
        map, second, BodySize, {}, tests::FixedStepSeconds, &aloneCost);
    REQUIRE(aloneCost.simulatedTicks == firstCost.simulatedTicks);
}

TEST_CASE("The cache keeps nothing for an invalid body", "[navigation][cache][validation]")
{
    PlatformerConnectionCache cache;
    const ConnectionBody flat{{12.0F, 0.0F}, {}, tests::FixedStepSeconds};
    const ConnectionBody stopped{BodySize, {}, 0.0F};
    REQUIRE_THROWS_AS(cache.keep({0, 0}, flat, {}, {{0, 0}, {0, 0}}), std::invalid_argument);
    REQUIRE_THROWS_AS(cache.keep({0, 0}, stopped, {}, {{0, 0}, {0, 0}}), std::invalid_argument);
    REQUIRE_THROWS_AS(cache.keepWalk(1, flat, {1, {}}), std::invalid_argument);
    REQUIRE_THROWS_AS(cache.keepReachable({0, 0}, stopped, {}), std::invalid_argument);
    REQUIRE_THROWS_AS(cache.keepPath({{0, 0}, {1, 0}, 0}, flat, {}), std::invalid_argument);
    REQUIRE_THROWS_AS(cache.queue({0, 0}, stopped), std::invalid_argument);
    REQUIRE(cache.size() == 0);
}
