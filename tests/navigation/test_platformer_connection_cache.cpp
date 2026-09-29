#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/navigation/platformer_connection_cache.hpp"
#include "simple_platformer/navigation/route.hpp"
#include "simple_platformer/navigation/platformer_connections.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/fixed_step.hpp"
#include "support/route_connections.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    using simple_platformer::BuiltPlatformerConnections;
    using simple_platformer::Cell;
    using simple_platformer::CellRange;
    using simple_platformer::PlatformerConnectionCache;
    using simple_platformer::PlatformerTraversalProfile;
    using simple_platformer::RouteConnection;
    using simple_platformer::WalkSimulationResult;

    constexpr glm::vec2 BodySize{12.0F, 12.0F};
}

TEST_CASE("Built connections enter the cache only when stored", "[navigation][cache]")
{
    // A ledge with a drop and a gap: walks, a fall and jumps leave the middle cell.
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "........", "###..###", "########"});
    const Cell cell{1, 2};
    PlatformerConnectionCache cache;
    const PlatformerTraversalProfile profile{BodySize, {}, tests::FixedStepSeconds};

    BuiltPlatformerConnections built =
        simple_platformer::buildPlatformerConnections(map, cell, profile, &cache);
    REQUIRE(cache.size() == 0);
    REQUIRE(cache.cachedWalkCount(profile) == 0);
    REQUIRE_FALSE(built.walksToCache.empty());
    const int simulatedTicks = built.simulatedTicks;
    simple_platformer::storePlatformerConnections(cache, cell, profile, std::move(built));
    const std::vector<RouteConnection>* simulated = cache.cachedConnections(cell, profile);
    REQUIRE(simulated != nullptr);
    REQUIRE_FALSE(simulated->empty());
    REQUIRE(simulatedTicks > 0);
    REQUIRE(cache.size() == 1);

    // Reading the stored cell requires no new simulation.
    REQUIRE(simulated == cache.cachedConnections(cell, profile));
    REQUIRE(cache.size() == 1);

    // Without a cache argument, the builder simulates the walks again.
    const BuiltPlatformerConnections uncached =
        simple_platformer::buildPlatformerConnections(map, cell, profile);
    REQUIRE(uncached.simulatedTicks == simulatedTicks);
}

TEST_CASE("Connections are cached separately for each profile", "[navigation][cache]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "########"});
    const Cell cell{3, 1};
    PlatformerConnectionCache cache;
    const PlatformerTraversalProfile profile{BodySize, {}, tests::FixedStepSeconds};
    BuiltPlatformerConnections initial =
        simple_platformer::buildPlatformerConnections(map, cell, profile);
    simple_platformer::storePlatformerConnections(cache, cell, profile, std::move(initial));
    REQUIRE(cache.cachedConnections(cell, profile) != nullptr);
    REQUIRE(cache.cachedConnections({0, 1}, profile) == nullptr);

    PlatformerTraversalProfile taller = profile;
    taller.size.y = 20.0F;
    REQUIRE(cache.cachedConnections(cell, taller) == nullptr);
    PlatformerTraversalProfile faster = profile;
    faster.movement.maximumSpeed += 1.0F;
    REQUIRE(cache.cachedConnections(cell, faster) == nullptr);
    PlatformerTraversalProfile finer = profile;
    finer.stepSeconds *= 0.5F;
    REQUIRE(cache.cachedConnections(cell, finer) == nullptr);
    PlatformerTraversalProfile climber = profile;
    climber.climb = simple_platformer::SurfaceClimbConfig{60.0F};
    REQUIRE(cache.cachedConnections(cell, climber) == nullptr);

    BuiltPlatformerConnections tallBuild =
        simple_platformer::buildPlatformerConnections(map, cell, taller, &cache);
    REQUIRE(tallBuild.simulatedTicks > 0);
    simple_platformer::storePlatformerConnections(cache, cell, taller, std::move(tallBuild));
    REQUIRE(cache.size() == 2);
    // The cache lists profiles in the order first seen.
    REQUIRE(cache.knownProfiles().size() == 2);
    REQUIRE(cache.knownProfiles()[0] == profile);
    REQUIRE(cache.knownProfiles()[1] == taller);

    cache.clear();
    REQUIRE(cache.size() == 0);
    REQUIRE(cache.cachedConnections(cell, profile) == nullptr);
}

TEST_CASE("A non-standable cell is cached with no connections", "[navigation][cache]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "########"});
    PlatformerConnectionCache cache;
    const PlatformerTraversalProfile profile{BodySize, {}, tests::FixedStepSeconds};
    BuiltPlatformerConnections built =
        simple_platformer::buildPlatformerConnections(map, {3, 0}, profile, &cache);
    simple_platformer::storePlatformerConnections(cache, {3, 0}, profile, std::move(built));
    const std::vector<RouteConnection>* connections = cache.cachedConnections({3, 0}, profile);
    REQUIRE(connections != nullptr);
    REQUIRE(connections->empty());
    REQUIRE(cache.size() == 1);
    REQUIRE(cache.cachedConnections({3, 0}, profile) == connections);
}

TEST_CASE("A walk is cached per length and profile, and a break leaves it", "[navigation][cache]")
{
    PlatformerConnectionCache cache;
    const PlatformerTraversalProfile profile{BodySize, {}, tests::FixedStepSeconds};
    const PlatformerTraversalProfile taller{{12.0F, 20.0F}, {}, tests::FixedStepSeconds};
    REQUIRE(cache.cachedWalk(3, profile) == nullptr);
    REQUIRE(cache.cachedWalkCount(profile) == 0);

    cache.storeWalk(profile, {3, 35, {{-1, -1}, {4, 1}}, 35});
    cache.storeWalk(profile, {-3, 36, {{-4, -1}, {1, 1}}});
    cache.storeWalk(profile, {12, std::nullopt, {{-1, -1}, {13, 1}}});
    REQUIRE(cache.cachedWalkCount(profile) == 3);
    REQUIRE(cache.cachedWalkCount(taller) == 0);
    REQUIRE(cache.cachedWalk(3, taller) == nullptr);
    const WalkSimulationResult* rightwards = cache.cachedWalk(3, profile);
    REQUIRE(rightwards != nullptr);
    REQUIRE(rightwards->columns == 3);
    REQUIRE(rightwards->cost.value_or(0) == 35);
    REQUIRE(rightwards->sweep.last == Cell{4, 1});
    REQUIRE(rightwards->simulatedTicks == 35);
    // Leftwards is its own length, and a walk past the limit is cached as such.
    REQUIRE(cache.cachedWalk(-3, profile)->cost.value_or(0) == 36);
    REQUIRE_FALSE(cache.cachedWalk(12, profile)->cost.has_value());

    // Storing again replaces; a break changes nothing, since no tile decided a walk.
    cache.storeWalk(profile, {3, 34, {{-1, -1}, {4, 1}}});
    REQUIRE(cache.cachedWalk(3, profile)->cost.value_or(0) == 34);
    cache.storeConnections({0, 1}, profile, {}, {{-2, 0}, {6, 2}});
    cache.invalidate({2, 1});
    REQUIRE(cache.cachedCellCount(profile) == 0);
    REQUIRE(cache.cachedWalkCount(profile) == 3);

    cache.clear();
    REQUIRE(cache.cachedWalkCount(profile) == 0);
}

TEST_CASE("Cached walks change nothing but the ticks simulated", "[navigation][cache]")
{
    // A floor long enough that a walk along it runs past the simulation limit.
    const std::string open(40, '.');
    const std::string floor(40, '#');
    const simple_platformer::TileMap map = tests::TileMapBuilder({open, open, floor});
    PlatformerConnectionCache cache;
    const PlatformerTraversalProfile profile{BodySize, {}, tests::FixedStepSeconds};
    const Cell first{20, 1};
    const Cell second{25, 1};

    // The first cell simulates every length it can walk, and the one it cannot.
    BuiltPlatformerConnections firstBuild =
        simple_platformer::buildPlatformerConnections(map, first, profile, &cache);
    const int firstSimulatedTicks = firstBuild.simulatedTicks;
    REQUIRE_FALSE(firstBuild.walksToCache.empty());
    REQUIRE(firstBuild.walksToCache.front().simulatedTicks > 0);
    simple_platformer::storePlatformerConnections(cache, first, profile, std::move(firstBuild));
    const std::size_t walks = cache.cachedWalkCount(profile);
    REQUIRE(walks > 0);
    bool pastTheLimit = false;
    for (int columns = -map.width(); columns <= map.width(); ++columns)
    {
        const WalkSimulationResult* walk = cache.cachedWalk(columns, profile);
        pastTheLimit = pastTheLimit || (walk != nullptr && !walk->cost.has_value());
    }
    REQUIRE(pastTheLimit);

    // Another cell of the floor walks the same lengths, so it simulates only its jumps
    // and falls and caches no new walk, yet its connections and footprint are the
    // ones it would have simulated alone.
    BuiltPlatformerConnections secondBuild =
        simple_platformer::buildPlatformerConnections(map, second, profile, &cache);
    const int secondSimulatedTicks = secondBuild.simulatedTicks;
    simple_platformer::storePlatformerConnections(cache, second, profile, std::move(secondBuild));
    const std::vector<RouteConnection>* cached = cache.cachedConnections(second, profile);
    REQUIRE(cached != nullptr);
    REQUIRE(secondSimulatedTicks < firstSimulatedTicks);
    REQUIRE(cache.cachedWalkCount(profile) == walks);
    PlatformerConnectionCache alone;
    BuiltPlatformerConnections aloneBuild =
        simple_platformer::buildPlatformerConnections(map, second, profile, &alone);
    simple_platformer::storePlatformerConnections(alone, second, profile, std::move(aloneBuild));
    const std::vector<RouteConnection>* simulated = alone.cachedConnections(second, profile);
    REQUIRE(simulated != nullptr);
    tests::requireSameRouteConnections(*cached, *simulated);
    const CellRange cachedFootprint = cache.cachedFootprint(second, profile).value_or(CellRange{});
    const CellRange simulatedFootprint =
        alone.cachedFootprint(second, profile).value_or(CellRange{});
    REQUIRE(cachedFootprint.first == simulatedFootprint.first);
    REQUIRE(cachedFootprint.last == simulatedFootprint.last);
    // Without a cache argument, the builder cannot reuse those walks.
    const BuiltPlatformerConnections aloneBuildWithoutCache =
        simple_platformer::buildPlatformerConnections(map, second, profile);
    REQUIRE(aloneBuildWithoutCache.simulatedTicks == firstSimulatedTicks);
}

TEST_CASE("The cache rejects an invalid profile", "[navigation][cache][validation]")
{
    PlatformerConnectionCache cache;
    const PlatformerTraversalProfile flat{{12.0F, 0.0F}, {}, tests::FixedStepSeconds};
    const PlatformerTraversalProfile stopped{BodySize, {}, 0.0F};
    REQUIRE_THROWS_AS(
        cache.storeConnections({0, 0}, flat, {}, {{0, 0}, {0, 0}}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        cache.storeConnections({0, 0}, stopped, {}, {{0, 0}, {0, 0}}), std::invalid_argument);
    REQUIRE_THROWS_AS(cache.storeWalk(flat, {1, 1, {}}), std::invalid_argument);
    REQUIRE_THROWS_AS(cache.queue({0, 0}, stopped), std::invalid_argument);
    REQUIRE(cache.size() == 0);
}
