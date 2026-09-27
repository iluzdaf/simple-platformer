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
#include "simple_platformer/world/tile_map.hpp"
#include "support/fixed_step.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    using simple_platformer::ConnectionBody;
    using simple_platformer::GridPosition;
    using simple_platformer::PathSearchStatistics;
    using simple_platformer::PlatformerConnectionCache;

    constexpr glm::vec2 BodySize{12.0F, 12.0F};

    void requireSameSteps(
        const std::optional<simple_platformer::NavigationPath>& left,
        const std::optional<simple_platformer::NavigationPath>& right)
    {
        const std::vector<simple_platformer::NavigationStep> leftSteps =
            left.value_or(simple_platformer::NavigationPath{}).steps;
        const std::vector<simple_platformer::NavigationStep> rightSteps =
            right.value_or(simple_platformer::NavigationPath{}).steps;
        REQUIRE(leftSteps.size() == rightSteps.size());
        for (std::size_t index = 0; index < leftSteps.size(); ++index)
        {
            REQUIRE(leftSteps[index].destinationCell == rightSteps[index].destinationCell);
            REQUIRE(leftSteps[index].traversal == rightSteps[index].traversal);
        }
    }
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
            map, start, goal, BodySize, {}, tests::FixedStepSeconds, {}, &uncached)
            .path;
    REQUIRE(expected.has_value());

    PlatformerConnectionCache cache;
    PathSearchStatistics filling;
    const std::optional<simple_platformer::NavigationPath> filled =
        simple_platformer::findPlatformerPath(
            map, start, goal, BodySize, {}, tests::FixedStepSeconds, {}, &filling, &cache)
            .path;
    REQUIRE(filled.has_value());
    requireSameSteps(filled, expected);
    // Filling simulates every cell it expands, though fewer ticks than a search without
    // a cache, which cannot remember a walk from one cell to the next.
    REQUIRE(filling.simulatedTicks > 0);
    REQUIRE(filling.simulatedTicks < uncached.simulatedTicks);
    REQUIRE(filling.cellsReused == 0);

    // A search from the next cell over expands only cells the first one kept.
    const GridPosition nextStart{1, 2};
    PathSearchStatistics uncachedNext;
    const std::optional<simple_platformer::NavigationPath> expectedNext =
        simple_platformer::findPlatformerPath(
            map, nextStart, goal, BodySize, {}, tests::FixedStepSeconds, {}, &uncachedNext)
            .path;
    PathSearchStatistics reusing;
    const std::optional<simple_platformer::NavigationPath> reused =
        simple_platformer::findPlatformerPath(
            map, nextStart, goal, BodySize, {}, tests::FixedStepSeconds, {}, &reusing, &cache)
            .path;
    REQUIRE(reused.has_value());
    requireSameSteps(reused, expectedNext);
    REQUIRE(reusing.simulatedTicks == 0);
    REQUIRE(reusing.nodesExpanded == uncachedNext.nodesExpanded);
    REQUIRE(reusing.cellsReused == reusing.nodesExpanded);
    REQUIRE(reusing.pathsRemembered == 0);
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
            .path.has_value());
    REQUIRE(failing.nodesExpanded > 0);
    const std::vector<GridPosition>* reachable = cache.reachableFrom(start, body);
    REQUIRE(reachable != nullptr);
    REQUIRE(std::find(reachable->begin(), reachable->end(), start) != reachable->end());
    REQUIRE(std::find(reachable->begin(), reachable->end(), farRight) != reachable->end());
    REQUIRE(std::find(reachable->begin(), reachable->end(), ledge) == reachable->end());
    // The set is kept per body, and is not a cell's connections.
    ConnectionBody taller = body;
    taller.size.y = 20.0F;
    REQUIRE(cache.reachableFrom(start, taller) == nullptr);
    REQUIRE(cache.reachableSetsKept(body) == 1);

    // The same failure again costs no expansion at all.
    PathSearchStatistics spared;
    REQUIRE_FALSE(simple_platformer::findPlatformerPath(
                      map, start, ledge, BodySize, {}, tests::FixedStepSeconds, {}, &spared, &cache)
                      .path.has_value());
    REQUIRE(spared.nodesExpanded == 0);
    REQUIRE(spared.cellsReused == 0);
    REQUIRE(spared.simulatedTicks == 0);

    // A goal the start leads to is still searched for and found.
    PathSearchStatistics reaching;
    REQUIRE(simple_platformer::findPlatformerPath(
                map, start, farRight, BodySize, {}, tests::FixedStepSeconds, {}, &reaching, &cache)
                .path.has_value());
    REQUIRE(reaching.nodesExpanded > 0);

    // A goal off the map is no path, and teaches nothing.
    PlatformerConnectionCache fresh;
    PathSearchStatistics offMap;
    REQUIRE_FALSE(
        simple_platformer::findPlatformerPath(
            map, start, {3, -1}, BodySize, {}, tests::FixedStepSeconds, {}, &offMap, &fresh)
            .path.has_value());
    REQUIRE(offMap.nodesExpanded == 0);
    REQUIRE(fresh.reachableFrom(start, body) == nullptr);

    // Without a cache the failure is searched every time.
    PathSearchStatistics uncached;
    REQUIRE_FALSE(simple_platformer::findPlatformerPath(
                      map, start, ledge, BodySize, {}, tests::FixedStepSeconds, {}, &uncached)
                      .path.has_value());
    REQUIRE(uncached.nodesExpanded == failing.nodesExpanded);

    cache.clear();
    REQUIRE(cache.reachableFrom(start, body) == nullptr);
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
            map, start, goal, BodySize, {}, tests::FixedStepSeconds, {30}, &first, &cache)
            .path;
    REQUIRE(found.has_value());
    REQUIRE(first.nodesExpanded > 0);
    REQUIRE(first.pathsRemembered == 0);
    REQUIRE(cache.pathKept(query, body) != nullptr);

    PathSearchStatistics second;
    const std::optional<simple_platformer::NavigationPath> remembered =
        simple_platformer::findPlatformerPath(
            map, start, goal, BodySize, {}, tests::FixedStepSeconds, {30}, &second, &cache)
            .path;
    REQUIRE(remembered.has_value());
    requireSameSteps(remembered, found);
    REQUIRE(second.nodesExpanded == 0);
    REQUIRE(second.cellsReused == 0);
    REQUIRE(second.pathsRemembered == 1);

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
