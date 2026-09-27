#include "simple_platformer/navigation/platformer_navigation.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
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
#include "simple_platformer/navigation/platformer_connections.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    namespace
    {
        using ConnectionVisitor = std::function<void(const NavigationNeighbor& neighbor)>;
        using ConnectionSource =
            std::function<void(GridPosition cell, const ConnectionVisitor& visit)>;

        std::optional<PlatformerPathResult> answerFromCache(
            const PlatformerConnectionCache& cache,
            const PathQuery& query,
            const ConnectionBody& body,
            PathSearchStatistics* statistics);
        ConnectionSource connectionsReadFrom(
            PlatformerConnectionCache& cache,
            const TileMap& map,
            const ConnectionBody& body,
            PathSearchStatistics* statistics,
            bool& incomplete);
        ConnectionSource connectionsSimulatedFor(
            const TileMap& map,
            const ConnectionBody& body,
            PathSearchStatistics* statistics);
        std::optional<NavigationPath> searchPlatformerPath(
            const TileMap& map,
            GridPosition start,
            GridPosition goal,
            const PlatformerMovementConfig& movement,
            float stepSeconds,
            const PlatformerNavigationConfig& navigation,
            const ConnectionSource& connectionsOf,
            PathSearchStatistics* statistics,
            std::vector<GridPosition>* reached);
        void keepWhatWasLearned(
            PlatformerConnectionCache& cache,
            const PathQuery& query,
            const ConnectionBody& body,
            const std::optional<NavigationPath>& path,
            std::vector<GridPosition> reached,
            bool incomplete);
    }

    PlatformerPathResult findPlatformerPath(
        const TileMap& map,
        GridPosition start,
        GridPosition goal,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement,
        float stepSeconds,
        const PlatformerNavigationConfig& navigation,
        PathSearchStatistics* statistics,
        PlatformerConnectionCache* cache)
    {
        requirePositiveSeconds(stepSeconds, "Navigation simulation step");
        if (navigation.jumpStartPenaltyTicks < 0)
        {
            throw std::invalid_argument("A jump start penalty cannot be negative");
        }
        if (!map.contains(start) || !map.contains(goal))
        {
            return {PlatformerPathStatus::Unreachable, std::nullopt};
        }
        const ConnectionBody body{bodySize, movement, stepSeconds};
        const PathQuery query{start, goal, navigation.jumpStartPenaltyTicks};

        if (cache != nullptr)
        {
            cache->syncWith(map);
            const std::optional<PlatformerPathResult> answer =
                answerFromCache(*cache, query, body, statistics);
            if (answer.has_value())
            {
                return answer.value();
            }
        }

        bool incomplete = false;
        const ConnectionSource connectionsOf =
            cache != nullptr ? connectionsReadFrom(*cache, map, body, statistics, incomplete)
                             : connectionsSimulatedFor(map, body, statistics);
        std::vector<GridPosition> reached;
        std::optional<NavigationPath> path = searchPlatformerPath(
            map,
            start,
            goal,
            movement,
            stepSeconds,
            navigation,
            connectionsOf,
            statistics,
            cache != nullptr ? &reached : nullptr);

        if (cache != nullptr)
        {
            keepWhatWasLearned(*cache, query, body, path, std::move(reached), incomplete);
        }
        if (path.has_value())
        {
            return {PlatformerPathStatus::Found, std::move(path)};
        }
        if (incomplete)
        {
            if (statistics != nullptr)
            {
                ++statistics->deferred;
            }
            return {PlatformerPathStatus::Deferred, std::nullopt};
        }
        return {PlatformerPathStatus::Unreachable, std::nullopt};
    }

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
        // The search itself, over whichever connections it is handed: a cell's
        // connections come from connectionsOf; a jump is charged its cost and the start
        // penalty. On failure, reached collects every discovered cell. The plain search
        // hands it connections simulated for this search alone; the cached search hands
        // it the cache's.
        std::optional<NavigationPath> searchPlatformerPath(
            const TileMap& map,
            GridPosition start,
            GridPosition goal,
            const PlatformerMovementConfig& movement,
            float stepSeconds,
            const PlatformerNavigationConfig& navigation,
            const ConnectionSource& connectionsOf,
            PathSearchStatistics* statistics,
            std::vector<GridPosition>* reached)
        {
            const GridNeighborFunction neighbors =
                [&navigation, &connectionsOf](GridPosition cell, const GridNeighborVisitor& visit)
            {
                connectionsOf(
                    cell,
                    [&](const NavigationNeighbor& neighbor)
                    {
                        if (neighbor.traversal != Traversal::Jump)
                        {
                            visit(neighbor, neighbor.cost);
                            return;
                        }
                        if (neighbor.cost >
                            std::numeric_limits<int>::max() - navigation.jumpStartPenaltyTicks)
                        {
                            throw std::overflow_error("A navigation connection cost is too large");
                        }
                        visit(neighbor, neighbor.cost + navigation.jumpStartPenaltyTicks);
                    });
            };
            const GridHeuristicFunction heuristic =
                [&map, &movement, stepSeconds](GridPosition cell, GridPosition goal)
            { return platformerTickHeuristic(map.tileSize(), cell, goal, movement, stepSeconds); };
            return findLowestCostPath(
                start, goal, map.size(), neighbors, heuristic, statistics, reached);
        }

        // What the cache can answer without a search, once it has one: a query it has
        // answered before gets the path it kept, and a goal outside the cells a failed
        // search from the start reached gets no path.
        std::optional<PlatformerPathResult> answerFromCache(
            const PlatformerConnectionCache& cache,
            const PathQuery& query,
            const ConnectionBody& body,
            PathSearchStatistics* statistics)
        {
            const NavigationPath* kept = cache.pathKept(query, body);
            if (kept != nullptr)
            {
                if (statistics != nullptr)
                {
                    ++statistics->pathsRemembered;
                }
                return PlatformerPathResult{PlatformerPathStatus::Found, *kept};
            }
            const std::vector<GridPosition>* reachable = cache.reachableFrom(query.start, body);
            if (reachable != nullptr &&
                std::find(reachable->begin(), reachable->end(), query.goal) == reachable->end())
            {
                return PlatformerPathResult{PlatformerPathStatus::Unreachable, std::nullopt};
            }
            return std::nullopt;
        }

        // The connections the search reads where the cache keeps them. A cell still
        // waiting for the fill is not simulated here: the search goes on without its
        // connections, the fill takes the cell next, and incomplete records the gap.
        ConnectionSource connectionsReadFrom(
            PlatformerConnectionCache& cache,
            const TileMap& map,
            const ConnectionBody& body,
            PathSearchStatistics* statistics,
            bool& incomplete)
        {
            return [&cache, &map, &body, statistics, &incomplete](
                       GridPosition cell, const ConnectionVisitor& visit)
            {
                if (cache.isPending(cell, body))
                {
                    cache.prioritise(cell, body);
                    incomplete = true;
                    return;
                }
                for (const NavigationNeighbor& neighbor : platformerNeighborsKept(
                         map, cell, body.size, body.movement, body.stepSeconds, cache, statistics))
                {
                    visit(neighbor);
                }
            };
        }

        ConnectionSource connectionsSimulatedFor(
            const TileMap& map,
            const ConnectionBody& body,
            PathSearchStatistics* statistics)
        {
            return [&map, &body, statistics](GridPosition cell, const ConnectionVisitor& visit)
            {
                for (const NavigationNeighbor& neighbor : platformerNeighbors(
                         map, cell, body.size, body.movement, body.stepSeconds, statistics))
                {
                    visit(neighbor);
                }
            };
        }

        // Keeps what a search learned: a found path, or the cells reached by a failed
        // one. A search that went without some cell's connections has learned nothing
        // the cache may keep, though a path it found is still valid.
        void keepWhatWasLearned(
            PlatformerConnectionCache& cache,
            const PathQuery& query,
            const ConnectionBody& body,
            const std::optional<NavigationPath>& path,
            std::vector<GridPosition> reached,
            bool incomplete)
        {
            if (incomplete)
            {
                return;
            }
            if (path.has_value())
            {
                cache.keepPath(query, body, *path);
            }
            else
            {
                cache.keepReachable(query.start, body, std::move(reached));
            }
        }
    }
}
