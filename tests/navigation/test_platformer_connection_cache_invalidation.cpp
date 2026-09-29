#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/navigation/platformer_connection_cache.hpp"
#include "simple_platformer/navigation/navigation_fill.hpp"
#include "simple_platformer/navigation/route.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/platformer_cells.hpp"
#include "simple_platformer/navigation/platformer_connections.hpp"
#include "simple_platformer/navigation/actor_navigation.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/timing/frame_profile.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/fixed_step.hpp"
#include "support/prepare_navigation_cache.hpp"
#include "support/route_connections.hpp"
#include "support/navigation_paths.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"

namespace
{
    using simple_platformer::Cell;
    using simple_platformer::FrameProfile;
    using simple_platformer::PlatformerConnectionCache;
    using simple_platformer::PlatformerTraversalProfile;
    using simple_platformer::RouteConnection;

    constexpr glm::vec2 BodySize{12.0F, 12.0F};
}

TEST_CASE("A break drops only the cells whose footprint holds it", "[navigation][cache]")
{
    PlatformerConnectionCache cache;
    const PlatformerTraversalProfile profile{BodySize, {}, tests::FixedStepSeconds};
    // Two cells: one swept the tile at (5, 1), the other never came near it.
    cache.storeConnections({0, 1}, profile, {}, {{0, 0}, {6, 2}});
    cache.storeConnections({9, 1}, profile, {}, {{8, 0}, {10, 2}});

    REQUIRE(cache.cachedCellCount(profile) == 2);
    REQUIRE(cache.cellsConnected(profile) == 0);
    cache.storeConnections(
        {9, 1},
        profile,
        {{{{10, 1}, simple_platformer::Traversal::Walk, {}}, 1}},
        {{8, 0}, {10, 2}});
    REQUIRE(cache.cellsConnected(profile) == 1);
    REQUIRE(cache.connectionWritesSoFar() == 3);

    cache.invalidate({5, 1});

    REQUIRE(cache.cachedConnections({0, 1}, profile) == nullptr);
    REQUIRE(cache.cachedConnections({9, 1}, profile) != nullptr);
    REQUIRE(cache.size() == 1);
    REQUIRE(cache.cachedCellCount(profile) == 1);
    REQUIRE(cache.cellsDroppedSoFar() == 1);

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
    REQUIRE(cache.nextPending(profile).value_or(Cell{}) == Cell{1, 1});
    cache.storeConnections({1, 1}, profile, {}, {{0, 0}, {6, 2}});
    REQUIRE(cache.cellsPending(profile) == 3);
    REQUIRE_FALSE(cache.isPending({1, 1}, profile));
    REQUIRE(cache.nextPending(profile).value_or(Cell{1, 1}) != Cell{1, 1});
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
    const Cell start{1, 1};
    const Cell goal{5, 1};
    simple_platformer::World world;
    world.addActor(tests::ActorBuilder::sized(BodySize)
                       .atFeet({24.0F, 32.0F})
                       .walking()
                       .thinking({64.0F, 1.0F}));
    const PlatformerTraversalProfile profile{BodySize, {}, tests::FixedStepSeconds};
    tests::prepareNavigationCache(map, world);
    PlatformerConnectionCache& cache = world.platformerConnections();
    const auto search = [&](FrameProfile& frame)
    {
        return simple_platformer::findActorPath(
                   map,
                   tests::actorFor(tests::restingBody({start}, profile), profile),
                   simple_platformer::feetInCell(tests::TileSize, goal),
                   profile.stepSeconds,
                   cache,
                   &frame)
            .value();
    };

    FrameProfile blocked;
    const auto blockedResult = search(blocked);
    REQUIRE(blockedResult.status == simple_platformer::NavigationPathStatus::Unreachable);
    REQUIRE(blockedResult.path.has_value());
    REQUIRE(
        simple_platformer::endOf(
            blockedResult.path.value_or(simple_platformer::NavigationPath{})) ==
        simple_platformer::feetInCell(tests::TileSize, {2, 1}));

    REQUIRE(map.breakTile({3, 1}));

    // The search applies the break, then waits at the dropped start instead of
    // simulating it. The start moves to the front of the fill queue.
    FrameProfile waiting;
    const auto waitingResult = search(waiting);
    REQUIRE(waitingResult.status == simple_platformer::NavigationPathStatus::Deferred);
    REQUIRE_FALSE(waitingResult.path.has_value());
    REQUIRE(simple_platformer::frameStatisticCount(waiting, "Paths deferred") == 1);
    REQUIRE(cache.cachedConnections(start, profile) == nullptr);
    REQUIRE(cache.nextPending(profile).value_or(Cell{}) == start);
    const std::size_t pending = cache.cellsPending(profile);
    REQUIRE(pending > 0);

    // A fill with ticks to spare recaches every dropped cell; the search then finds the
    // route through the gap without simulating.
    FrameProfile fillProfile;
    const int cellsCached =
        simple_platformer::advanceNavigationFill(map, cache, 1000000, &fillProfile);
    REQUIRE(cellsCached == static_cast<int>(pending));
    REQUIRE(simple_platformer::frameStatisticCount(fillProfile, "Fill simulated ticks") > 0);
    REQUIRE(cache.cellsPending(profile) == 0);
    FrameProfile opened;
    const auto openedResult = search(opened);
    REQUIRE(openedResult.status == simple_platformer::NavigationPathStatus::Found);
    REQUIRE(openedResult.path.has_value());
    REQUIRE(simple_platformer::frameStatisticCount(opened, "Paths deferred") == 0);
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
    const auto walksTo = [](const std::vector<RouteConnection>& connections, Cell cell)
    {
        return std::any_of(
            connections.begin(),
            connections.end(),
            [cell](const RouteConnection& connection)
            {
                return connection.step.traversal == simple_platformer::Traversal::Walk &&
                       connection.step.destinationCell == cell;
            });
    };
    REQUIRE(walksTo(*cache.cachedConnections({1, 0}, profile), {6, 0}));
    const std::vector<RouteConnection>* farAway = cache.cachedConnections({23, 0}, profile);
    REQUIRE(farAway != nullptr);
    const std::vector<RouteConnection> farAwayBefore = *farAway;
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
    const std::vector<RouteConnection>* afterBreak = cache.cachedConnections({1, 0}, profile);
    REQUIRE(afterBreak != nullptr);
    REQUIRE_FALSE(walksTo(*afterBreak, {6, 0}));
    auto edgeBuild = simple_platformer::buildPlatformerConnections(map, {2, 0}, profile, &cache);
    simple_platformer::storePlatformerConnections(cache, {2, 0}, profile, std::move(edgeBuild));
    const std::vector<RouteConnection>* fromTheEdge = cache.cachedConnections({2, 0}, profile);
    REQUIRE(fromTheEdge != nullptr);
    REQUIRE(std::any_of(
        fromTheEdge->begin(),
        fromTheEdge->end(),
        [](const RouteConnection& connection)
        {
            return connection.step.traversal == simple_platformer::Traversal::Fall &&
                   connection.step.destinationCell == Cell{3, 2};
        }));
    // A cell whose simulations never came near the hole was left as it was.
    const std::vector<RouteConnection>* farAwayAfter = cache.cachedConnections({23, 0}, profile);
    REQUIRE(farAwayAfter != nullptr);
    tests::requireSameRouteConnections(*farAwayAfter, farAwayBefore);
    REQUIRE(cache.connectionWritesSoFar() == writesBeforeBreak + 2);
}

TEST_CASE("A broken climbable tile takes its climbs away", "[navigation][cache][climb]")
{
    using simple_platformer::ClimbSurface;
    using simple_platformer::RouteLocation;
    simple_platformer::TileMap map =
        tests::TileMapBuilder({"......", ".c....", ".g....", ".c....", "######"})
            .where('c', tests::Tile().blocksMovement().climbable())
            .where('g', tests::Tile().blocksMovement().climbable().breaksInto('.'));
    const PlatformerTraversalProfile climber{BodySize, {}, tests::FixedStepSeconds, {{60.0F}}};
    const RouteLocation start{{2, 3}};
    const RouteLocation onWall{{2, 1}, ClimbSurface::LeftWall};
    const glm::vec2 wallFeet = simple_platformer::feetOf(
        simple_platformer::boundsAtSurface(tests::TileSize, onWall, BodySize));
    PlatformerConnectionCache cache;
    tests::fillConnections(map, cache, climber);
    const auto search = [&]()
    {
        return simple_platformer::findActorPath(
                   map,
                   tests::actorFor(tests::restingBody(start, climber), climber),
                   wallFeet,
                   climber.stepSeconds,
                   cache)
            .value();
    };

    const auto climbed = search();
    REQUIRE(climbed.status == simple_platformer::NavigationPathStatus::Found);
    REQUIRE(
        simple_platformer::endOf(climbed.path.value_or(simple_platformer::NavigationPath{})) ==
        wallFeet);

    REQUIRE(map.breakTile({1, 2}));

    // The cells that climbed past the tile wait for the fill; then the wall above
    // the gap is out of reach, and the path stops in the cell below it.
    REQUIRE(search().status == simple_platformer::NavigationPathStatus::Deferred);
    simple_platformer::advanceNavigationFill(map, cache, 1000000);
    const auto stopped = search();
    REQUIRE(stopped.status == simple_platformer::NavigationPathStatus::Unreachable);
    REQUIRE(
        simple_platformer::cellAtFeet(
            tests::TileSize,
            simple_platformer::endOf(stopped.path.value_or(simple_platformer::NavigationPath{}))) ==
        Cell{2, 3});
}
