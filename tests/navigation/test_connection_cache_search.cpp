#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/connection_cache.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_search.hpp"
#include "simple_platformer/navigation/platformer_cells.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/fixed_step.hpp"
#include "support/require_same_input_program.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    using simple_platformer::GridPosition;
    using simple_platformer::PathSearchStatistics;
    using simple_platformer::PlatformerConnectionCache;
    using simple_platformer::PlatformerTraversalProfile;

    constexpr glm::vec2 BodySize{12.0F, 12.0F};
    constexpr GridPosition LedgeStart{0, 6};
    constexpr GridPosition UnreachableLedge{3, 0};
    constexpr GridPosition ReachableFloorCell{7, 6};

    simple_platformer::TileMap ledgeBeyondJumpRange()
    {
        return tests::TileMapBuilder(
            {"........",
             "..###...",
             "........",
             "........",
             "........",
             "........",
             "........",
             "########"});
    }

    simple_platformer::NavigationPathResult searchForLedge(
        const simple_platformer::TileMap& map,
        PlatformerConnectionCache& cache,
        PathSearchStatistics* statistics = nullptr)
    {
        return simple_platformer::findPlatformerPath(
            map,
            LedgeStart,
            UnreachableLedge,
            BodySize,
            {},
            tests::FixedStepSeconds,
            cache,
            {},
            statistics);
    }

    void requireSameSteps(
        const simple_platformer::NavigationPath& left,
        const simple_platformer::NavigationPath& right)
    {
        REQUIRE(left.start == right.start);
        const std::vector<simple_platformer::NavigationStep>& leftSteps = left.steps;
        const std::vector<simple_platformer::NavigationStep>& rightSteps = right.steps;
        REQUIRE(leftSteps.size() == rightSteps.size());
        for (std::size_t index = 0; index < leftSteps.size(); ++index)
        {
            REQUIRE(leftSteps[index].destinationCell == rightSteps[index].destinationCell);
            REQUIRE(leftSteps[index].traversal == rightSteps[index].traversal);
            tests::requireSameInputProgram(leftSteps[index].inputs, rightSteps[index].inputs);
        }
    }
}

TEST_CASE("A nearby search reuses connections from an earlier path", "[navigation][cache]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "........", "###..###", "########"});
    const GridPosition start{0, 2};
    const GridPosition goal{7, 2};
    PlatformerConnectionCache cache;
    PathSearchStatistics filling;
    const std::optional<simple_platformer::NavigationPath> filled =
        simple_platformer::findPlatformerPath(
            map, start, goal, BodySize, {}, tests::FixedStepSeconds, cache, {}, &filling)
            .path;
    REQUIRE(filled.has_value());
    REQUIRE(filling.simulatedTicks > 0);
    REQUIRE(filling.cellsReused == 0);

    // A search from the next cell over expands only cells the first one cached.
    const GridPosition nextStart{1, 2};
    PathSearchStatistics reusing;
    const std::optional<simple_platformer::NavigationPath> reused =
        simple_platformer::findPlatformerPath(
            map, nextStart, goal, BodySize, {}, tests::FixedStepSeconds, cache, {}, &reusing)
            .path;
    REQUIRE(reused.has_value());
    REQUIRE(reusing.simulatedTicks == 0);
    REQUIRE(reusing.nodesExpanded > 0);
    REQUIRE(reusing.cellsReused == reusing.nodesExpanded);
    REQUIRE(reusing.pathsRemembered == 0);
}

TEST_CASE("What a failed search learned spares the next one", "[navigation][cache]")
{
    const simple_platformer::TileMap map = ledgeBeyondJumpRange();
    PlatformerConnectionCache cache;
    const PlatformerTraversalProfile profile{BodySize, {}, tests::FixedStepSeconds};
    REQUIRE(simple_platformer::canStandAt(map, UnreachableLedge, BodySize));
    REQUIRE(cache.cachedReachableCells(LedgeStart, profile) == nullptr);

    PathSearchStatistics failing;
    const auto first = searchForLedge(map, cache, &failing);
    REQUIRE(first.status == simple_platformer::NavigationPathStatus::Unreachable);
    REQUIRE(failing.nodesExpanded > 0);
    const std::vector<GridPosition>* reachable = cache.cachedReachableCells(LedgeStart, profile);
    REQUIRE(reachable != nullptr);
    REQUIRE(std::find(reachable->begin(), reachable->end(), LedgeStart) != reachable->end());
    REQUIRE(
        std::find(reachable->begin(), reachable->end(), ReachableFloorCell) != reachable->end());
    REQUIRE(std::find(reachable->begin(), reachable->end(), UnreachableLedge) == reachable->end());

    PathSearchStatistics spared;
    const auto repeated = searchForLedge(map, cache, &spared);
    REQUIRE(repeated.status == simple_platformer::NavigationPathStatus::Unreachable);
    REQUIRE(spared.nodesExpanded == 0);
    REQUIRE(spared.cellsReused == 0);
    REQUIRE(spared.simulatedTicks == 0);
}

TEST_CASE("Reachable goals are still searched after a cached failure", "[navigation][cache]")
{
    const simple_platformer::TileMap map = ledgeBeyondJumpRange();
    PlatformerConnectionCache cache;
    REQUIRE(
        searchForLedge(map, cache).status == simple_platformer::NavigationPathStatus::Unreachable);

    PathSearchStatistics reaching;
    const auto result = simple_platformer::findPlatformerPath(
        map,
        LedgeStart,
        ReachableFloorCell,
        BodySize,
        {},
        tests::FixedStepSeconds,
        cache,
        {},
        &reaching);
    REQUIRE(result.status == simple_platformer::NavigationPathStatus::Found);
    REQUIRE(reaching.nodesExpanded > 0);
}

TEST_CASE("Reachable sets belong to their traversal profile", "[navigation][cache]")
{
    PlatformerConnectionCache cache;
    const GridPosition start{0, 0};
    const PlatformerTraversalProfile profile{BodySize, {}, tests::FixedStepSeconds};
    PlatformerTraversalProfile taller = profile;
    taller.size.y = 20.0F;

    cache.storeReachableCells(start, profile, {start, {1, 0}});
    REQUIRE(cache.cachedReachableCells(start, profile) != nullptr);
    REQUIRE(cache.cachedReachableCells(start, taller) == nullptr);
    REQUIRE(cache.cachedReachableSetCount(profile) == 1);
    REQUIRE(cache.cachedReachableSetCount(taller) == 0);
}

TEST_CASE("An off-map goal does not cache a failed search", "[navigation][cache]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"...", "###"});
    const GridPosition start{0, 0};
    const PlatformerTraversalProfile profile{BodySize, {}, tests::FixedStepSeconds};
    PlatformerConnectionCache cache;
    PathSearchStatistics offMap;
    const auto result = simple_platformer::findPlatformerPath(
        map, start, {3, -1}, BodySize, {}, tests::FixedStepSeconds, cache, {}, &offMap);
    REQUIRE(result.status == simple_platformer::NavigationPathStatus::Unreachable);
    REQUIRE(offMap.nodesExpanded == 0);
    REQUIRE(cache.cachedReachableCells(start, profile) == nullptr);
}

TEST_CASE("A cold cache repeats an unreachable search", "[navigation][cache]")
{
    const simple_platformer::TileMap map = ledgeBeyondJumpRange();
    PlatformerConnectionCache firstCache;
    PlatformerConnectionCache secondCache;
    PathSearchStatistics first;
    PathSearchStatistics second;
    REQUIRE(
        searchForLedge(map, firstCache, &first).status ==
        simple_platformer::NavigationPathStatus::Unreachable);
    REQUIRE(
        searchForLedge(map, secondCache, &second).status ==
        simple_platformer::NavigationPathStatus::Unreachable);
    REQUIRE(first.nodesExpanded > 0);
    REQUIRE(second.nodesExpanded == first.nodesExpanded);
}

TEST_CASE("Clearing the cache removes learned reachable cells", "[navigation][cache]")
{
    PlatformerConnectionCache cache;
    const GridPosition start{0, 0};
    const PlatformerTraversalProfile profile{BodySize, {}, tests::FixedStepSeconds};
    cache.storeReachableCells(start, profile, {start});
    REQUIRE(cache.cachedReachableCells(start, profile) != nullptr);
    cache.clear();
    REQUIRE(cache.cachedReachableCells(start, profile) == nullptr);
}

TEST_CASE("A found path answers the same search again without expanding", "[navigation][cache]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "........", "###..###", "########"});
    const GridPosition start{0, 2};
    const GridPosition goal{7, 2};
    PlatformerConnectionCache cache;
    const PlatformerTraversalProfile profile{BodySize, {}, tests::FixedStepSeconds};
    const simple_platformer::PathQuery query{start, goal, 30};
    REQUIRE(cache.cachedPath(query, profile) == nullptr);

    PathSearchStatistics first;
    const std::optional<simple_platformer::NavigationPath> found =
        simple_platformer::findPlatformerPath(
            map, start, goal, BodySize, {}, tests::FixedStepSeconds, cache, {30}, &first)
            .path;
    REQUIRE(found.has_value());
    REQUIRE(first.nodesExpanded > 0);
    REQUIRE(first.pathsRemembered == 0);
    REQUIRE(cache.cachedPath(query, profile) != nullptr);

    PathSearchStatistics second;
    const std::optional<simple_platformer::NavigationPath> remembered =
        simple_platformer::findPlatformerPath(
            map, start, goal, BodySize, {}, tests::FixedStepSeconds, cache, {30}, &second)
            .path;
    REQUIRE(remembered.has_value());
    requireSameSteps(
        remembered.value_or(simple_platformer::NavigationPath{}),
        found.value_or(simple_platformer::NavigationPath{}));
    REQUIRE(second.nodesExpanded == 0);
    REQUIRE(second.cellsReused == 0);
    REQUIRE(second.pathsRemembered == 1);

    // Another goal, or another penalty, is a new search.
    PathSearchStatistics elsewhere;
    simple_platformer::findPlatformerPath(
        map, start, {6, 2}, BodySize, {}, tests::FixedStepSeconds, cache, {30}, &elsewhere);
    REQUIRE(elsewhere.nodesExpanded > 0);
    PathSearchStatistics penalised;
    simple_platformer::findPlatformerPath(
        map, start, goal, BodySize, {}, tests::FixedStepSeconds, cache, {0}, &penalised);
    REQUIRE(penalised.nodesExpanded > 0);
    REQUIRE(penalised.pathsRemembered == 0);

    cache.clear();
    REQUIRE(cache.cachedPath(query, profile) == nullptr);
}

TEST_CASE(
    "A pending cell defers a search even when an expensive route is available",
    "[navigation][cache]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"...."});
    const GridPosition start{0, 0};
    const GridPosition pending{1, 0};
    const GridPosition goal{2, 0};
    const GridPosition unrelated{3, 0};
    const PlatformerTraversalProfile profile{BodySize, {}, tests::FixedStepSeconds};
    const simple_platformer::PathQuery query{start, goal, 30};
    PlatformerConnectionCache cache;
    cache.storeConnections(
        start,
        profile,
        {{{pending, simple_platformer::Traversal::Walk, {}}, 1},
         {{goal, simple_platformer::Traversal::Walk, {}}, 100}},
        {start, goal});
    cache.queue(unrelated, profile);
    cache.queue(pending, profile);
    REQUIRE(cache.nextPending(profile) == unrelated);

    PathSearchStatistics waiting;
    const auto deferred = simple_platformer::findPlatformerPath(
        map, start, goal, BodySize, {}, tests::FixedStepSeconds, cache, {}, &waiting);
    REQUIRE(deferred.status == simple_platformer::NavigationPathStatus::Deferred);
    REQUIRE_FALSE(deferred.path.has_value());
    REQUIRE(waiting.deferred == 1);
    REQUIRE(waiting.nodesExpanded == 1);
    REQUIRE(cache.nextPending(profile) == pending);
    REQUIRE(cache.cachedPath(query, profile) == nullptr);
    REQUIRE(cache.cachedReachableCells(start, profile) == nullptr);

    cache.storeConnections(
        pending, profile, {{{goal, simple_platformer::Traversal::Walk, {}}, 1}}, {pending, goal});
    const auto found = simple_platformer::findPlatformerPath(
        map, start, goal, BodySize, {}, tests::FixedStepSeconds, cache);
    REQUIRE(found.status == simple_platformer::NavigationPathStatus::Found);
    REQUIRE(found.path.has_value());
    const simple_platformer::NavigationPath route =
        found.path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(route.steps.size() == 2);
    REQUIRE(route.steps.front().destinationCell == pending);
}
