#include "simple_platformer/navigation/platformer_navigation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/connection_cache.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_search.hpp"
#include "simple_platformer/navigation/platformer_cells.hpp"
#include "simple_platformer/navigation/platformer_connections.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    int platformerTickHeuristic(
        int tileSize,
        GridPosition cell,
        GridPosition goal,
        const PlatformerMovementConfig& movement,
        float stepSeconds)
    {
        requirePositiveSeconds(stepSeconds, "Navigation simulation step");
        if (!std::isfinite(movement.maximumSpeed) || movement.maximumSpeed < 0.0F)
        {
            throw std::invalid_argument(
                "Platformer navigation maximum speed must be finite and non-negative");
        }

        const int columnDistance = std::abs(goal.x - cell.x);
        if (columnDistance == 0 || movement.maximumSpeed == 0.0F)
        {
            return 0;
        }

        // Reaching any point inside the goal column is sufficient. Ignoring acceleration,
        // braking, obstacles, and vertical travel keeps this estimate optimistic.
        const float minimumDistance =
            (static_cast<float>(columnDistance) - 0.5F) * static_cast<float>(tileSize);
        const float maximumDistancePerTick = movement.maximumSpeed * stepSeconds;
        return static_cast<int>(std::ceil(minimumDistance / maximumDistancePerTick));
    }

    namespace
    {
        // A remembered path answers its exact query; a failed search can also rule out
        // a goal outside the cells it reached from the same start.
        std::optional<NavigationPathResult> tryAnswerFromCache(
            const PlatformerConnectionCache& cache,
            const PathQuery& query,
            const PlatformerTraversalProfile& profile,
            PathSearchStatistics* statistics)
        {
            const NavigationPath* cached = cache.cachedPath(query, profile);
            if (cached != nullptr)
            {
                if (statistics != nullptr)
                {
                    ++statistics->pathsRemembered;
                }
                return NavigationPathResult{NavigationPathStatus::Found, *cached};
            }
            const std::vector<GridPosition>* reachable =
                cache.cachedReachableCells(query.start, profile);
            if (reachable != nullptr &&
                std::find(reachable->begin(), reachable->end(), query.goal) == reachable->end())
            {
                return NavigationPathResult{NavigationPathStatus::Unreachable, std::nullopt};
            }
            return std::nullopt;
        }

        // Adjust this search's copy, not the simulated travel costs shared by the cache.
        void applyJumpStartPenalty(std::vector<NavigationConnection>& connections, int penaltyTicks)
        {
            for (NavigationConnection& connection : connections)
            {
                if (connection.step.traversal != Traversal::Jump)
                {
                    continue;
                }
                if (connection.cost > std::numeric_limits<int>::max() - penaltyTicks)
                {
                    throw std::overflow_error("A navigation connection cost is too large");
                }
                connection.cost += penaltyTicks;
            }
        }

        PathSearchResult searchPlatformerPath(
            const TileMap& map,
            const PathQuery& query,
            const PlatformerTraversalProfile& profile,
            PathSearchStatistics* statistics,
            PlatformerConnectionCache& cache)
        {
            const GridNeighborFunction neighbors = [&map, &profile](GridPosition cell)
            { return platformerNeighbors(map, cell, profile.size); };
            const GridConnectionFunction connections =
                [&map, &query, &profile, statistics, &cache](GridPosition cell)
            {
                if (statistics != nullptr)
                {
                    ++statistics->nodesExpanded;
                }
                const std::vector<NavigationConnection>* cached =
                    cache.cachedConnections(cell, profile);
                if (cached == nullptr)
                {
                    BuiltPlatformerConnections built =
                        buildPlatformerConnections(map, cell, profile, &cache);
                    if (statistics != nullptr)
                    {
                        statistics->simulatedTicks += built.simulatedTicks;
                    }
                    storePlatformerConnections(cache, cell, profile, std::move(built));
                    cached = cache.cachedConnections(cell, profile);
                }
                else if (statistics != nullptr)
                {
                    ++statistics->cellsReused;
                }
                if (cached == nullptr)
                {
                    throw std::logic_error("Platformer connections were not cached");
                }
                std::vector<NavigationConnection> connections = *cached;
                applyJumpStartPenalty(connections, query.jumpStartPenaltyTicks);
                return connections;
            };
            const GridHeuristicFunction heuristic =
                [&map, &profile](GridPosition cell, GridPosition goal)
            {
                return platformerTickHeuristic(
                    map.tileSize(), cell, goal, profile.movement, profile.stepSeconds);
            };
            const GridExpansionReady canExpand = [&cache, &profile](GridPosition cell)
            { return !cache.isPending(cell, profile); };
            return findLowestCostPath(
                query.start, query.goal, map.size(), neighbors, connections, heuristic, canExpand);
        }

        void cacheSearchResult(
            PlatformerConnectionCache& cache,
            const PathQuery& query,
            const PlatformerTraversalProfile& profile,
            PathSearchResult& result)
        {
            if (result.path.has_value())
            {
                cache.storePath(query, profile, result.path.value());
            }
            else
            {
                cache.storeReachableCells(query.start, profile, std::move(result.reachableCells));
            }
        }
    }

    NavigationPathResult findPlatformerPath(
        const TileMap& map,
        GridPosition start,
        GridPosition goal,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement,
        float stepSeconds,
        PlatformerConnectionCache& cache,
        const PlatformerNavigationConfig& navigation,
        PathSearchStatistics* statistics)
    {
        requirePositiveSeconds(stepSeconds, "Navigation simulation step");
        if (navigation.jumpStartPenaltyTicks < 0)
        {
            throw std::invalid_argument("A jump start penalty cannot be negative");
        }
        if (!map.contains(start) || !map.contains(goal))
        {
            return {NavigationPathStatus::Unreachable, std::nullopt};
        }
        const PlatformerTraversalProfile profile{bodySize, movement, stepSeconds};
        const PathQuery query{start, goal, navigation.jumpStartPenaltyTicks};

        cache.applyRecordedTileBreaks(map);
        const std::optional<NavigationPathResult> answer =
            tryAnswerFromCache(cache, query, profile, statistics);
        if (answer.has_value())
        {
            return answer.value();
        }
        PathSearchResult result = searchPlatformerPath(map, query, profile, statistics, cache);

        if (result.status == PathSearchStatus::Incomplete)
        {
            if (!result.unexpandedCell.has_value())
            {
                throw std::logic_error("Incomplete path search did not identify a cell");
            }
            cache.prioritise(result.unexpandedCell.value(), profile);
            if (statistics != nullptr)
            {
                ++statistics->deferred;
            }
            return {NavigationPathStatus::Deferred, std::nullopt};
        }
        cacheSearchResult(cache, query, profile, result);
        if (result.status == PathSearchStatus::Found)
        {
            return {NavigationPathStatus::Found, std::move(result.path)};
        }
        return {NavigationPathStatus::Unreachable, std::nullopt};
    }
}
