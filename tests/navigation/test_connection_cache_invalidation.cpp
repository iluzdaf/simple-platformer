#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/connection_cache.hpp"
#include "simple_platformer/navigation/navigation_fill.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_search.hpp"
#include "simple_platformer/navigation/platformer_connections.hpp"
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

    constexpr glm::vec2 BodySize{12.0F, 12.0F};
}

TEST_CASE("A break drops only the cells whose footprint holds it", "[navigation][cache]")
{
    PlatformerConnectionCache cache;
    const ConnectionBody body{BodySize, {}, tests::FixedStepSeconds};
    // Two cells: one swept the tile at (5, 1), the other never came near it.
    cache.keep({0, 1}, body, {}, {{0, 0}, {6, 2}});
    cache.keep({9, 1}, body, {}, {{8, 0}, {10, 2}});
    cache.keepReachable({0, 1}, body, {{0, 1}, {1, 1}});
    cache.keepReachable({9, 1}, body, {{9, 1}});
    const simple_platformer::PathQuery query{{9, 1}, {9, 1}, 0};
    cache.keepPath(query, body, {{9, 1}, {}});

    REQUIRE(cache.cellsKept(body) == 2);
    REQUIRE(cache.cellsConnected(body) == 0);
    cache.keep(
        {9, 1}, body, {{{10, 1}, simple_platformer::Traversal::Walk, 1, {}}}, {{8, 0}, {10, 2}});
    REQUIRE(cache.cellsConnected(body) == 1);
    REQUIRE(cache.reachableSetsKept(body) == 2);
    REQUIRE(cache.pathsKept(body) == 1);
    REQUIRE(cache.cellsKeptSoFar() == 3);

    cache.invalidate({5, 1});

    REQUIRE(cache.find({0, 1}, body) == nullptr);
    REQUIRE(cache.find({9, 1}, body) != nullptr);
    REQUIRE(cache.size() == 1);
    REQUIRE(cache.cellsKept(body) == 1);
    REQUIRE(cache.reachableSetsKept(body) == 1);
    REQUIRE(cache.pathsKept(body) == 0);
    REQUIRE(cache.cellsDroppedSoFar() == 1);
    // A reachable set that held the dropped cell goes; one that did not stays.
    REQUIRE(cache.reachableFrom({0, 1}, body) == nullptr);
    REQUIRE(cache.reachableFrom({9, 1}, body) != nullptr);
    // Every remembered path goes, since a new opening can make a cheaper route anywhere.
    REQUIRE(cache.pathKept(query, body) == nullptr);

    // Keeping counts up; clearing forgets the counts with the rest.
    cache.keep({0, 1}, body, {}, {{0, 0}, {6, 2}});
    REQUIRE(cache.cellsKeptSoFar() == 4);
    cache.clear();
    REQUIRE(cache.cellsKeptSoFar() == 0);
    REQUIRE(cache.cellsDroppedSoFar() == 0);
    REQUIRE(cache.breaksApplied() == 0);
}

TEST_CASE("Dropped and queued cells wait in one queue, each once", "[navigation][cache]")
{
    PlatformerConnectionCache cache;
    const ConnectionBody body{BodySize, {}, tests::FixedStepSeconds};
    const ConnectionBody other{{12.0F, 20.0F}, {}, tests::FixedStepSeconds};
    REQUIRE(cache.cellsPending(body) == 0);
    REQUIRE_FALSE(cache.nextPending(body).has_value());

    // Three cells swept the tile at (5, 1), for the one body; one for the other.
    cache.keep({0, 1}, body, {}, {{0, 0}, {6, 2}});
    cache.keep({1, 1}, body, {}, {{0, 0}, {6, 2}});
    cache.keep({2, 1}, body, {}, {{0, 0}, {6, 2}});
    cache.keep({0, 1}, other, {}, {{0, 0}, {6, 2}});
    cache.invalidate({5, 1});
    REQUIRE(cache.cellsPending(body) == 3);
    REQUIRE(cache.cellsPending(other) == 1);
    REQUIRE(cache.isPending({1, 1}, body));
    REQUIRE_FALSE(cache.isPending({1, 1}, other));

    // A queued cell joins behind them; one already kept or waiting is not queued again.
    cache.queue({7, 1}, body);
    cache.queue({7, 1}, body);
    cache.queue({1, 1}, body);
    cache.keep({8, 1}, body, {}, {{8, 0}, {8, 2}});
    cache.queue({8, 1}, body);
    REQUIRE(cache.cellsPending(body) == 4);
    REQUIRE(cache.isPending({7, 1}, body));
    REQUIRE_FALSE(cache.isPending({8, 1}, body));

    // A cell moved to the front comes next; keeping a cell takes it off the queue.
    cache.prioritise({1, 1}, body);
    REQUIRE(cache.nextPending(body).value_or(GridPosition{}) == GridPosition{1, 1});
    cache.keep({1, 1}, body, {}, {{0, 0}, {6, 2}});
    REQUIRE(cache.cellsPending(body) == 3);
    REQUIRE_FALSE(cache.isPending({1, 1}, body));
    REQUIRE(cache.nextPending(body).value_or(GridPosition{1, 1}) != GridPosition{1, 1});
    // Moving a cell that is not waiting, or one of a body never kept, changes nothing.
    cache.prioritise({1, 1}, body);
    cache.prioritise({0, 1}, {{1.0F, 1.0F}, {}, tests::FixedStepSeconds});
    REQUIRE(cache.cellsPending(body) == 3);

    cache.clear();
    REQUIRE(cache.cellsPending(body) == 0);
}

TEST_CASE("Syncing with the map applies each break once", "[navigation][cache]")
{
    simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "###g####"})
            .where('g', tests::Tile().blocksMovement().breaksInto('.'));
    PlatformerConnectionCache cache;
    const ConnectionBody body{BodySize, {}, tests::FixedStepSeconds};
    cache.keep({3, 0}, body, {}, {{2, 0}, {4, 1}});
    cache.syncWith(map);
    REQUIRE(cache.find({3, 0}, body) != nullptr);

    REQUIRE(map.breakTile({3, 1}));
    cache.syncWith(map);
    REQUIRE(cache.find({3, 0}, body) == nullptr);
    REQUIRE(cache.breaksApplied() == 1);

    // Kept again after the break, the cell stays through later syncs of the same log.
    cache.keep({3, 0}, body, {}, {{2, 0}, {4, 1}});
    cache.syncWith(map);
    REQUIRE(cache.find({3, 0}, body) != nullptr);
}

TEST_CASE("A broken wall opens a route once the fill has caught up", "[navigation][cache]")
{
    // A corridor one cell tall with a breakable wall across it: no jump gets over.
    simple_platformer::TileMap map =
        tests::TileMapBuilder({"########", "#..g...#", "########"})
            .where('g', tests::Tile().blocksMovement().breaksInto('.'));
    const GridPosition start{1, 1};
    const GridPosition goal{5, 1};
    PlatformerConnectionCache cache;
    const ConnectionBody body{BodySize, {}, tests::FixedStepSeconds};
    simple_platformer::keepAllPlatformerConnections(
        map, BodySize, {}, tests::FixedStepSeconds, cache);
    const auto search = [&](PathSearchStatistics& statistics)
    {
        return simple_platformer::findPlatformerPath(
            map, start, goal, BodySize, {}, tests::FixedStepSeconds, {}, &statistics, &cache);
    };

    PathSearchStatistics blocked;
    const auto blockedResult = search(blocked);
    REQUIRE(blockedResult.status == simple_platformer::PlatformerPathStatus::Unreachable);
    REQUIRE_FALSE(blockedResult.path.has_value());
    REQUIRE(blocked.simulatedTicks == 0);
    REQUIRE(cache.reachableFrom(start, body) != nullptr);

    REQUIRE(map.breakTile({3, 1}));

    // The search syncs with the map, meets the dropped start and waits rather than
    // simulate it: nothing is simulated or kept, and the start is first in line.
    PathSearchStatistics waiting;
    const auto waitingResult = search(waiting);
    REQUIRE(waitingResult.status == simple_platformer::PlatformerPathStatus::Deferred);
    REQUIRE_FALSE(waitingResult.path.has_value());
    REQUIRE(waiting.deferred == 1);
    REQUIRE(waiting.simulatedTicks == 0);
    REQUIRE(cache.find(start, body) == nullptr);
    REQUIRE(cache.reachableFrom(start, body) == nullptr);
    REQUIRE(cache.nextPending(body).value_or(GridPosition{}) == start);
    const std::size_t pending = cache.cellsPending(body);
    REQUIRE(pending > 0);

    // A fill with ticks to spare keeps every dropped cell; the search then finds the
    // route through the gap without simulating.
    const simple_platformer::FillWork work = simple_platformer::fillPlatformerConnections(
        map, BodySize, {}, tests::FixedStepSeconds, cache, 1000000);
    REQUIRE(work.cells == static_cast<int>(pending));
    REQUIRE(work.simulatedTicks > 0);
    REQUIRE(cache.cellsPending(body) == 0);
    PathSearchStatistics opened;
    const auto openedResult = search(opened);
    REQUIRE(openedResult.status == simple_platformer::PlatformerPathStatus::Found);
    REQUIRE(openedResult.path.has_value());
    REQUIRE(opened.deferred == 0);
    REQUIRE(opened.simulatedTicks == 0);
    REQUIRE(opened.pathsRemembered == 0);
}

TEST_CASE("A broken floor takes a walk away and gives a fall", "[navigation][cache]")
{
    // An upper floor with a breakable tile, over a lower floor that catches a fall.
    simple_platformer::TileMap map =
        tests::TileMapBuilder({"........................",
                               "###g####################",
                               "........................",
                               "########################"})
            .where('g', tests::Tile().blocksMovement().breaksInto('.'));
    PlatformerConnectionCache cache;
    const ConnectionBody body{BodySize, {}, tests::FixedStepSeconds};
    simple_platformer::keepAllPlatformerConnections(
        map, BodySize, {}, tests::FixedStepSeconds, cache);
    const auto walksTo = [](const std::vector<NavigationNeighbor>& connections, GridPosition cell)
    {
        return std::any_of(
            connections.begin(),
            connections.end(),
            [cell](const NavigationNeighbor& neighbor)
            {
                return neighbor.traversal == simple_platformer::Traversal::Walk &&
                       neighbor.destinationCell == cell;
            });
    };
    REQUIRE(walksTo(*cache.find({1, 0}, body), {6, 0}));
    const std::vector<NavigationNeighbor>* farAway = cache.find({23, 0}, body);
    REQUIRE(farAway != nullptr);

    REQUIRE(map.breakTile({3, 1}));

    // The walk across the hole is gone, and a fall into it has appeared.
    const std::vector<NavigationNeighbor>& afterBreak = simple_platformer::platformerNeighborsKept(
        map, {1, 0}, BodySize, {}, tests::FixedStepSeconds, cache);
    REQUIRE_FALSE(walksTo(afterBreak, {6, 0}));
    const std::vector<NavigationNeighbor>& fromTheEdge = simple_platformer::platformerNeighborsKept(
        map, {2, 0}, BodySize, {}, tests::FixedStepSeconds, cache);
    REQUIRE(std::any_of(
        fromTheEdge.begin(),
        fromTheEdge.end(),
        [](const NavigationNeighbor& neighbor)
        {
            return neighbor.traversal == simple_platformer::Traversal::Fall &&
                   neighbor.destinationCell == GridPosition{3, 2};
        }));
    // A cell whose simulations never came near the hole was left as it was.
    REQUIRE(cache.find({23, 0}, body) == farAway);
}
