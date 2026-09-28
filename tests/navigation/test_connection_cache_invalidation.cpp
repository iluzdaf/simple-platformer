#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/connection_cache.hpp"
#include "simple_platformer/navigation/navigation_fill.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_search.hpp"
#include "simple_platformer/navigation/platformer_connections.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/fixed_step.hpp"
#include "support/prepare_navigation_cache.hpp"
#include "support/require_same_navigation_connections.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    using simple_platformer::GridPosition;
    using simple_platformer::NavigationConnection;
    using simple_platformer::PathSearchStatistics;
    using simple_platformer::PlatformerConnectionCache;
    using simple_platformer::PlatformerTraversalProfile;

    constexpr glm::vec2 BodySize{12.0F, 12.0F};
}

TEST_CASE("A break drops only the cells whose footprint holds it", "[navigation][cache]")
{
    PlatformerConnectionCache cache;
    const PlatformerTraversalProfile profile{BodySize, {}, tests::FixedStepSeconds};
    // Two cells: one swept the tile at (5, 1), the other never came near it.
    cache.storeConnections({0, 1}, profile, {}, {{0, 0}, {6, 2}});
    cache.storeConnections({9, 1}, profile, {}, {{8, 0}, {10, 2}});
    cache.storeReachableCells({0, 1}, profile, {{0, 1}, {1, 1}});
    cache.storeReachableCells({9, 1}, profile, {{9, 1}});
    const simple_platformer::PathQuery query{{9, 1}, {9, 1}, 0};
    cache.storePath(query, profile, {{9, 1}, {}});

    REQUIRE(cache.cachedCellCount(profile) == 2);
    REQUIRE(cache.cellsConnected(profile) == 0);
    cache.storeConnections(
        {9, 1},
        profile,
        {{{{10, 1}, simple_platformer::Traversal::Walk, {}}, 1}},
        {{8, 0}, {10, 2}});
    REQUIRE(cache.cellsConnected(profile) == 1);
    REQUIRE(cache.cachedReachableSetCount(profile) == 2);
    REQUIRE(cache.cachedPathCount(profile) == 1);
    REQUIRE(cache.connectionWritesSoFar() == 3);

    cache.invalidate({5, 1});

    REQUIRE(cache.cachedConnections({0, 1}, profile) == nullptr);
    REQUIRE(cache.cachedConnections({9, 1}, profile) != nullptr);
    REQUIRE(cache.size() == 1);
    REQUIRE(cache.cachedCellCount(profile) == 1);
    REQUIRE(cache.cachedReachableSetCount(profile) == 1);
    REQUIRE(cache.cachedPathCount(profile) == 0);
    REQUIRE(cache.cellsDroppedSoFar() == 1);
    // A reachable set that held the dropped cell goes; one that did not stays.
    REQUIRE(cache.cachedReachableCells({0, 1}, profile) == nullptr);
    REQUIRE(cache.cachedReachableCells({9, 1}, profile) != nullptr);
    // Every remembered path goes, since a new opening can make a cheaper route anywhere.
    REQUIRE(cache.cachedPath(query, profile) == nullptr);

    cache.storeConnections({0, 1}, profile, {}, {{0, 0}, {6, 2}});
    REQUIRE(cache.connectionWritesSoFar() == 4);
    cache.clear();
    REQUIRE(cache.connectionWritesSoFar() == 0);
    REQUIRE(cache.cellsDroppedSoFar() == 0);
    REQUIRE(cache.breaksApplied() == 0);
}

TEST_CASE("Dropped and queued cells wait in one queue, each once", "[navigation][cache]")
{
    PlatformerConnectionCache cache;
    const PlatformerTraversalProfile profile{BodySize, {}, tests::FixedStepSeconds};
    const PlatformerTraversalProfile other{{12.0F, 20.0F}, {}, tests::FixedStepSeconds};
    REQUIRE(cache.cellsPending(profile) == 0);
    REQUIRE_FALSE(cache.nextPending(profile).has_value());

    // Three cells swept the tile at (5, 1), for the one profile; one for the other.
    cache.storeConnections({0, 1}, profile, {}, {{0, 0}, {6, 2}});
    cache.storeConnections({1, 1}, profile, {}, {{0, 0}, {6, 2}});
    cache.storeConnections({2, 1}, profile, {}, {{0, 0}, {6, 2}});
    cache.storeConnections({0, 1}, other, {}, {{0, 0}, {6, 2}});
    cache.invalidate({5, 1});
    REQUIRE(cache.cellsPending(profile) == 3);
    REQUIRE(cache.cellsPending(other) == 1);
    REQUIRE(cache.isPending({1, 1}, profile));
    REQUIRE_FALSE(cache.isPending({1, 1}, other));

    // A queued cell joins behind them; one already cached or waiting is not queued again.
    cache.queue({7, 1}, profile);
    cache.queue({7, 1}, profile);
    cache.queue({1, 1}, profile);
    cache.storeConnections({8, 1}, profile, {}, {{8, 0}, {8, 2}});
    cache.queue({8, 1}, profile);
    REQUIRE(cache.cellsPending(profile) == 4);
    REQUIRE(cache.isPending({7, 1}, profile));
    REQUIRE_FALSE(cache.isPending({8, 1}, profile));

    // A cell moved to the front comes next; caching a cell takes it off the queue.
    cache.prioritise({1, 1}, profile);
    REQUIRE(cache.nextPending(profile).value_or(GridPosition{}) == GridPosition{1, 1});
    cache.storeConnections({1, 1}, profile, {}, {{0, 0}, {6, 2}});
    REQUIRE(cache.cellsPending(profile) == 3);
    REQUIRE_FALSE(cache.isPending({1, 1}, profile));
    REQUIRE(cache.nextPending(profile).value_or(GridPosition{1, 1}) != GridPosition{1, 1});
    // Moving a cell that is not waiting, or one of an unknown profile, changes nothing.
    cache.prioritise({1, 1}, profile);
    cache.prioritise({0, 1}, {{1.0F, 1.0F}, {}, tests::FixedStepSeconds});
    REQUIRE(cache.cellsPending(profile) == 3);

    cache.clear();
    REQUIRE(cache.cellsPending(profile) == 0);
}

TEST_CASE("Syncing with the map applies each break once", "[navigation][cache]")
{
    simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "###g####"})
            .where('g', tests::Tile().blocksMovement().breaksInto('.'));
    PlatformerConnectionCache cache;
    const PlatformerTraversalProfile profile{BodySize, {}, tests::FixedStepSeconds};
    cache.storeConnections({3, 0}, profile, {}, {{2, 0}, {4, 1}});
    cache.applyRecordedTileBreaks(map);
    REQUIRE(cache.cachedConnections({3, 0}, profile) != nullptr);

    REQUIRE(map.breakTile({3, 1}));
    cache.applyRecordedTileBreaks(map);
    REQUIRE(cache.cachedConnections({3, 0}, profile) == nullptr);
    REQUIRE(cache.breaksApplied() == 1);

    // Reapplying the same break log leaves the recached cell intact.
    cache.storeConnections({3, 0}, profile, {}, {{2, 0}, {4, 1}});
    cache.applyRecordedTileBreaks(map);
    REQUIRE(cache.cachedConnections({3, 0}, profile) != nullptr);
}

TEST_CASE("A broken wall opens a route once the fill has caught up", "[navigation][cache]")
{
    // A corridor one cell tall with a breakable wall across it: no jump gets over.
    simple_platformer::TileMap map =
        tests::TileMapBuilder({"########", "#..g...#", "########"})
            .where('g', tests::Tile().blocksMovement().breaksInto('.'));
    const GridPosition start{1, 1};
    const GridPosition goal{5, 1};
    simple_platformer::World world;
    world.addActor(tests::ActorBuilder::sized(BodySize)
                       .atFeet({24.0F, 32.0F})
                       .walking()
                       .thinking({64.0F, 1.0F}));
    const PlatformerTraversalProfile profile{BodySize, {}, tests::FixedStepSeconds};
    tests::prepareNavigationCache(map, world);
    PlatformerConnectionCache& cache = world.platformerConnections();
    const auto search = [&](PathSearchStatistics& statistics)
    {
        return simple_platformer::findPlatformerPath(
            map, start, goal, BodySize, {}, tests::FixedStepSeconds, cache, {}, &statistics);
    };

    PathSearchStatistics blocked;
    const auto blockedResult = search(blocked);
    REQUIRE(blockedResult.status == simple_platformer::NavigationPathStatus::Unreachable);
    REQUIRE_FALSE(blockedResult.path.has_value());
    REQUIRE(blocked.simulatedTicks == 0);
    REQUIRE(cache.cachedReachableCells(start, profile) != nullptr);

    REQUIRE(map.breakTile({3, 1}));

    // The search applies the break, then waits at the dropped start instead of
    // simulating it. The start moves to the front of the fill queue.
    PathSearchStatistics waiting;
    const auto waitingResult = search(waiting);
    REQUIRE(waitingResult.status == simple_platformer::NavigationPathStatus::Deferred);
    REQUIRE_FALSE(waitingResult.path.has_value());
    REQUIRE(waiting.deferred == 1);
    REQUIRE(waiting.simulatedTicks == 0);
    REQUIRE(cache.cachedConnections(start, profile) == nullptr);
    REQUIRE(cache.cachedReachableCells(start, profile) == nullptr);
    REQUIRE(cache.nextPending(profile).value_or(GridPosition{}) == start);
    const std::size_t pending = cache.cellsPending(profile);
    REQUIRE(pending > 0);

    // A fill with ticks to spare recaches every dropped cell; the search then finds the
    // route through the gap without simulating.
    const simple_platformer::NavigationFillStatistics fillStatistics =
        simple_platformer::advanceNavigationFill(map, cache, 1000000);
    REQUIRE(fillStatistics.cellsCached == static_cast<int>(pending));
    REQUIRE(fillStatistics.simulatedTicks > 0);
    REQUIRE(cache.cellsPending(profile) == 0);
    PathSearchStatistics opened;
    const auto openedResult = search(opened);
    REQUIRE(openedResult.status == simple_platformer::NavigationPathStatus::Found);
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
    simple_platformer::World world;
    world.addActor(tests::ActorBuilder::sized(BodySize)
                       .atFeet({24.0F, 16.0F})
                       .walking()
                       .thinking({64.0F, 1.0F}));
    const PlatformerTraversalProfile profile{BodySize, {}, tests::FixedStepSeconds};
    tests::prepareNavigationCache(map, world);
    PlatformerConnectionCache& cache = world.platformerConnections();
    const auto walksTo = [](const std::vector<NavigationConnection>& connections, GridPosition cell)
    {
        return std::any_of(
            connections.begin(),
            connections.end(),
            [cell](const NavigationConnection& connection)
            {
                return connection.step.traversal == simple_platformer::Traversal::Walk &&
                       connection.step.destinationCell == cell;
            });
    };
    REQUIRE(walksTo(*cache.cachedConnections({1, 0}, profile), {6, 0}));
    const std::vector<NavigationConnection>* farAway = cache.cachedConnections({23, 0}, profile);
    REQUIRE(farAway != nullptr);
    const std::vector<NavigationConnection> farAwayBefore = *farAway;
    REQUIRE_FALSE(farAwayBefore.empty());
    const std::size_t writesBeforeBreak = cache.connectionWritesSoFar();
    const std::size_t dropsBeforeBreak = cache.cellsDroppedSoFar();

    REQUIRE(map.breakTile({3, 1}));

    // The walk across the hole is gone, and a fall into it has appeared.
    cache.applyRecordedTileBreaks(map);
    REQUIRE(cache.connectionWritesSoFar() == writesBeforeBreak);
    REQUIRE(cache.cellsDroppedSoFar() > dropsBeforeBreak);
    auto firstBuild = simple_platformer::buildPlatformerConnections(map, {1, 0}, profile, &cache);
    simple_platformer::storePlatformerConnections(cache, {1, 0}, profile, std::move(firstBuild));
    const std::vector<NavigationConnection>* afterBreak = cache.cachedConnections({1, 0}, profile);
    REQUIRE(afterBreak != nullptr);
    REQUIRE_FALSE(walksTo(*afterBreak, {6, 0}));
    auto edgeBuild = simple_platformer::buildPlatformerConnections(map, {2, 0}, profile, &cache);
    simple_platformer::storePlatformerConnections(cache, {2, 0}, profile, std::move(edgeBuild));
    const std::vector<NavigationConnection>* fromTheEdge = cache.cachedConnections({2, 0}, profile);
    REQUIRE(fromTheEdge != nullptr);
    REQUIRE(std::any_of(
        fromTheEdge->begin(),
        fromTheEdge->end(),
        [](const NavigationConnection& connection)
        {
            return connection.step.traversal == simple_platformer::Traversal::Fall &&
                   connection.step.destinationCell == GridPosition{3, 2};
        }));
    // A cell whose simulations never came near the hole was left as it was.
    const std::vector<NavigationConnection>* farAwayAfter =
        cache.cachedConnections({23, 0}, profile);
    REQUIRE(farAwayAfter != nullptr);
    tests::requireSameNavigationConnections(*farAwayAfter, farAwayBefore);
    REQUIRE(cache.connectionWritesSoFar() == writesBeforeBreak + 2);
}
