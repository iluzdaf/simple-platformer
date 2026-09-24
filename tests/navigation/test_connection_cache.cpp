#include <catch2/catch_test_macros.hpp>

#include <algorithm>
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

TEST_CASE("Kept connections can be read where the cache holds them", "[navigation][cache]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "########"});
    const GridPosition cell{3, 1};
    PlatformerConnectionCache cache;
    PathSearchStatistics first;
    PathSearchStatistics second;

    const std::vector<NavigationNeighbor>& kept = simple_platformer::platformerNeighborsKept(
        map, cell, BodySize, {}, tests::FixedStepSeconds, cache, &first);
    REQUIRE(first.simulatedTicks > 0);
    REQUIRE(first.cellsReused == 0);
    const std::vector<NavigationNeighbor>& again = simple_platformer::platformerNeighborsKept(
        map, cell, BodySize, {}, tests::FixedStepSeconds, cache, &second);
    // The same vector, not a copy of it, and nothing simulated to give it.
    REQUIRE(&again == &kept);
    REQUIRE(second.simulatedTicks == 0);
    REQUIRE(second.cellsReused == 1);
    REQUIRE(&again == cache.find(cell, {BodySize, {}, tests::FixedStepSeconds}));
    requireSameConnections(
        kept,
        simple_platformer::platformerNeighbors(map, cell, BodySize, {}, tests::FixedStepSeconds));
}

TEST_CASE("Keeping every cell leaves a search nothing to simulate", "[navigation][cache]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "........", "###..###", "########"});
    PlatformerConnectionCache cache;
    simple_platformer::keepAllPlatformerConnections(
        map, BodySize, {}, tests::FixedStepSeconds, cache);
    REQUIRE(
        cache.size() ==
        static_cast<std::size_t>(map.width()) * static_cast<std::size_t>(map.height()));

    PathSearchStatistics statistics;
    REQUIRE(simple_platformer::findPlatformerPath(
                map, {0, 2}, {7, 2}, BodySize, {}, tests::FixedStepSeconds, {}, &statistics, &cache)
                .has_value());
    REQUIRE(statistics.simulatedTicks == 0);
    REQUIRE(statistics.cellsReused == statistics.nodesExpanded);

    // Keeping again changes nothing.
    simple_platformer::keepAllPlatformerConnections(
        map, BodySize, {}, tests::FixedStepSeconds, cache);
    REQUIRE(
        cache.size() ==
        static_cast<std::size_t>(map.width()) * static_cast<std::size_t>(map.height()));
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

    // A search from the next cell over expands only cells the first one kept.
    const GridPosition nextStart{1, 2};
    PathSearchStatistics uncachedNext;
    const std::optional<simple_platformer::NavigationPath> expectedNext =
        simple_platformer::findPlatformerPath(
            map, nextStart, goal, BodySize, {}, tests::FixedStepSeconds, {}, &uncachedNext);
    PathSearchStatistics reusing;
    const std::optional<simple_platformer::NavigationPath> reused =
        simple_platformer::findPlatformerPath(
            map, nextStart, goal, BodySize, {}, tests::FixedStepSeconds, {}, &reusing, &cache);
    REQUIRE(reused.has_value());
    REQUIRE(reusing.simulatedTicks == 0);
    REQUIRE(reusing.nodesExpanded == uncachedNext.nodesExpanded);
    REQUIRE(reusing.cellsReused == reusing.nodesExpanded);
    REQUIRE(reusing.pathsRemembered == 0);

    const std::vector<simple_platformer::NavigationStep> expectedSteps =
        expectedNext.value_or(simple_platformer::NavigationPath{}).steps;
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

TEST_CASE("What a failed search learned spares the next one", "[navigation][cache]")
{
    // A ledge six tiles above the floor, far beyond any jump.
    const simple_platformer::TileMap map = tests::TileMapBuilder(
        {"........",
         "..###...",
         "........",
         "........",
         "........",
         "........",
         "........",
         "########"});
    const GridPosition start{0, 6};
    const GridPosition ledge{3, 0};
    const GridPosition farRight{7, 6};
    PlatformerConnectionCache cache;
    const ConnectionBody body{BodySize, {}, tests::FixedStepSeconds};
    REQUIRE(simple_platformer::canStandAt(map, ledge, BodySize));
    REQUIRE(cache.reachableFrom(start, body) == nullptr);

    PathSearchStatistics failing;
    REQUIRE_FALSE(
        simple_platformer::findPlatformerPath(
            map, start, ledge, BodySize, {}, tests::FixedStepSeconds, {}, &failing, &cache)
            .has_value());
    REQUIRE(failing.nodesExpanded > 0);
    const std::vector<GridPosition>* reachable = cache.reachableFrom(start, body);
    REQUIRE(reachable != nullptr);
    REQUIRE(std::find(reachable->begin(), reachable->end(), start) != reachable->end());
    REQUIRE(std::find(reachable->begin(), reachable->end(), farRight) != reachable->end());
    REQUIRE(std::find(reachable->begin(), reachable->end(), ledge) == reachable->end());

    // The same failure again costs no expansion at all.
    PathSearchStatistics spared;
    REQUIRE_FALSE(simple_platformer::findPlatformerPath(
                      map, start, ledge, BodySize, {}, tests::FixedStepSeconds, {}, &spared, &cache)
                      .has_value());
    REQUIRE(spared.nodesExpanded == 0);
    REQUIRE(spared.cellsReused == 0);
    REQUIRE(spared.simulatedTicks == 0);

    // A goal the start leads to is still searched for and found.
    PathSearchStatistics reaching;
    REQUIRE(simple_platformer::findPlatformerPath(
                map, start, farRight, BodySize, {}, tests::FixedStepSeconds, {}, &reaching, &cache)
                .has_value());
    REQUIRE(reaching.nodesExpanded > 0);

    // A goal off the map is no path, and teaches nothing.
    PlatformerConnectionCache fresh;
    PathSearchStatistics offMap;
    REQUIRE_FALSE(
        simple_platformer::findPlatformerPath(
            map, start, {3, -1}, BodySize, {}, tests::FixedStepSeconds, {}, &offMap, &fresh)
            .has_value());
    REQUIRE(offMap.nodesExpanded == 0);
    REQUIRE(fresh.reachableFrom(start, body) == nullptr);

    // Without a cache the failure is searched every time, as before.
    PathSearchStatistics uncached;
    REQUIRE_FALSE(simple_platformer::findPlatformerPath(
                      map, start, ledge, BodySize, {}, tests::FixedStepSeconds, {}, &uncached)
                      .has_value());
    REQUIRE(uncached.nodesExpanded == failing.nodesExpanded);
}

TEST_CASE("A found path answers the same search again without expanding", "[navigation][cache]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "........", "###..###", "########"});
    const GridPosition start{0, 2};
    const GridPosition goal{7, 2};
    PlatformerConnectionCache cache;
    const ConnectionBody body{BodySize, {}, tests::FixedStepSeconds};
    const simple_platformer::PathQuery query{start, goal, 30};
    REQUIRE(cache.pathKept(query, body) == nullptr);

    PathSearchStatistics first;
    const std::optional<simple_platformer::NavigationPath> found =
        simple_platformer::findPlatformerPath(
            map, start, goal, BodySize, {}, tests::FixedStepSeconds, {30}, &first, &cache);
    REQUIRE(found.has_value());
    REQUIRE(first.nodesExpanded > 0);
    REQUIRE(first.pathsRemembered == 0);
    REQUIRE(cache.pathKept(query, body) != nullptr);

    PathSearchStatistics second;
    const std::optional<simple_platformer::NavigationPath> remembered =
        simple_platformer::findPlatformerPath(
            map, start, goal, BodySize, {}, tests::FixedStepSeconds, {30}, &second, &cache);
    REQUIRE(remembered.has_value());
    REQUIRE(second.nodesExpanded == 0);
    REQUIRE(second.cellsReused == 0);
    REQUIRE(second.pathsRemembered == 1);
    const std::vector<simple_platformer::NavigationStep> foundSteps =
        found.value_or(simple_platformer::NavigationPath{}).steps;
    const std::vector<simple_platformer::NavigationStep> rememberedSteps =
        remembered.value_or(simple_platformer::NavigationPath{}).steps;
    REQUIRE(rememberedSteps.size() == foundSteps.size());
    for (std::size_t index = 0; index < foundSteps.size(); ++index)
    {
        REQUIRE(rememberedSteps[index].destinationCell == foundSteps[index].destinationCell);
        REQUIRE(rememberedSteps[index].traversal == foundSteps[index].traversal);
        REQUIRE(rememberedSteps[index].inputs.size() == foundSteps[index].inputs.size());
    }

    // Another goal, or another penalty, is a new search.
    PathSearchStatistics elsewhere;
    simple_platformer::findPlatformerPath(
        map, start, {6, 2}, BodySize, {}, tests::FixedStepSeconds, {30}, &elsewhere, &cache);
    REQUIRE(elsewhere.nodesExpanded > 0);
    PathSearchStatistics penalised;
    simple_platformer::findPlatformerPath(
        map, start, goal, BodySize, {}, tests::FixedStepSeconds, {0}, &penalised, &cache);
    REQUIRE(penalised.nodesExpanded > 0);
    REQUIRE(penalised.pathsRemembered == 0);

    cache.clear();
    REQUIRE(cache.pathKept(query, body) == nullptr);
}

TEST_CASE("Reachable cells are kept per start and body until cleared", "[navigation][cache]")
{
    PlatformerConnectionCache cache;
    const ConnectionBody body{BodySize, {}, tests::FixedStepSeconds};
    cache.keepReachable({0, 0}, body, {{0, 0}, {1, 0}});
    REQUIRE(cache.reachableFrom({0, 0}, body) != nullptr);
    REQUIRE(cache.reachableFrom({0, 0}, body)->size() == 2);
    REQUIRE(cache.reachableFrom({1, 0}, body) == nullptr);
    ConnectionBody taller = body;
    taller.size.y = 20.0F;
    REQUIRE(cache.reachableFrom({0, 0}, taller) == nullptr);
    // Reachable cells are not connections.
    REQUIRE(cache.size() == 0);

    cache.clear();
    REQUIRE(cache.reachableFrom({0, 0}, body) == nullptr);
    ConnectionBody stopped{BodySize, {}, 0.0F};
    REQUIRE_THROWS_AS(cache.keepReachable({0, 0}, stopped, {}), std::invalid_argument);
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

TEST_CASE("A break queues the cells it dropped until they are kept again", "[navigation][cache]")
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
    REQUIRE(cache.nextPending(body).has_value());

    // A cell moved to the front comes next; keeping a cell takes it off the queue.
    cache.prioritise({1, 1}, body);
    REQUIRE(cache.nextPending(body).value_or(GridPosition{}) == GridPosition{1, 1});
    cache.keep({1, 1}, body, {}, {{0, 0}, {6, 2}});
    REQUIRE(cache.cellsPending(body) == 2);
    REQUIRE_FALSE(cache.isPending({1, 1}, body));
    REQUIRE(cache.nextPending(body).value_or(GridPosition{1, 1}) != GridPosition{1, 1});
    // A cell that is not waiting, or a body never kept, changes nothing.
    cache.prioritise({1, 1}, body);
    cache.prioritise({0, 1}, {{1.0F, 1.0F}, {}, tests::FixedStepSeconds});
    REQUIRE(cache.cellsPending(body) == 2);

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

TEST_CASE("A broken wall opens a route once the refill has caught up", "[navigation][cache]")
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
    REQUIRE_FALSE(search(blocked).has_value());
    REQUIRE(blocked.simulatedTicks == 0);
    REQUIRE(cache.reachableFrom(start, body) != nullptr);

    REQUIRE(map.breakTile({3, 1}));

    // The search meets the dropped start and waits rather than simulate it: nothing is
    // simulated or kept, and the start is first in line for the refill.
    PathSearchStatistics waiting;
    REQUIRE_FALSE(search(waiting).has_value());
    REQUIRE(waiting.deferred == 1);
    REQUIRE(waiting.simulatedTicks == 0);
    REQUIRE(cache.find(start, body) == nullptr);
    REQUIRE(cache.reachableFrom(start, body) == nullptr);
    REQUIRE(cache.nextPending(body).value_or(GridPosition{}) == start);
    const std::size_t pending = cache.cellsPending(body);
    REQUIRE(pending > 0);

    // A refill with ticks to spare keeps every dropped cell; the search then finds the
    // route through the gap without simulating.
    const simple_platformer::RefillWork work = simple_platformer::refillPlatformerConnections(
        map, BodySize, {}, tests::FixedStepSeconds, cache, 1000000);
    REQUIRE(work.cells == static_cast<int>(pending));
    REQUIRE(work.simulatedTicks > 0);
    REQUIRE(cache.cellsPending(body) == 0);
    PathSearchStatistics opened;
    REQUIRE(search(opened).has_value());
    REQUIRE(opened.deferred == 0);
    REQUIRE(opened.simulatedTicks == 0);
    REQUIRE(opened.pathsRemembered == 0);
}

TEST_CASE("A refill keeps one cell at a time until its budget is spent", "[navigation][cache]")
{
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
    const auto refill = [&](int tickBudget)
    {
        return simple_platformer::refillPlatformerConnections(
            map, BodySize, {}, tests::FixedStepSeconds, cache, tickBudget);
    };

    // A refill syncs with the map itself, like a search.
    REQUIRE(map.breakTile({3, 1}));
    REQUIRE(refill(0).cells == 0);
    const std::size_t pending = cache.cellsPending(body);
    REQUIRE(pending > 1);

    // A one-tick budget stops after the first cell that simulates anything.
    const simple_platformer::RefillWork first = refill(1);
    REQUIRE(first.cells >= 1);
    REQUIRE(first.simulatedTicks >= 1);
    REQUIRE(cache.cellsPending(body) == pending - static_cast<std::size_t>(first.cells));
    REQUIRE(
        cache.cellsKeptSoFar() == static_cast<std::size_t>(map.width() * map.height()) +
                                      static_cast<std::size_t>(first.cells));

    // The rest go with ticks to spare, and a refill with nothing waiting does nothing.
    const simple_platformer::RefillWork rest = refill(1000000);
    REQUIRE(
        static_cast<std::size_t>(rest.cells) == pending - static_cast<std::size_t>(first.cells));
    REQUIRE(cache.cellsPending(body) == 0);
    REQUIRE(refill(1000000).cells == 0);

    REQUIRE_THROWS_AS(refill(-1), std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::refillPlatformerConnections(map, BodySize, {}, 0.0F, cache, 1),
        std::invalid_argument);
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

TEST_CASE("Connections are kept only for a valid body", "[navigation][cache][validation]")
{
    PlatformerConnectionCache cache;
    ConnectionBody flat{{12.0F, 0.0F}, {}, tests::FixedStepSeconds};
    REQUIRE_THROWS_AS(cache.keep({0, 0}, flat, {}, {{0, 0}, {0, 0}}), std::invalid_argument);
    ConnectionBody stopped{BodySize, {}, 0.0F};
    REQUIRE_THROWS_AS(cache.keep({0, 0}, stopped, {}, {{0, 0}, {0, 0}}), std::invalid_argument);
    REQUIRE(cache.size() == 0);
}
