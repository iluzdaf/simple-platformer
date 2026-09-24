#include "simple_platformer/navigation/platformer_navigation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/connection_cache.hpp"
#include "simple_platformer/navigation/input_program.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/path_search.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    namespace
    {
        // Two seconds at 60 Hz. A traversal that has not landed and stopped by then is not
        // a connection.
        constexpr int MaximumConnectionSimulationTicks = 120;

        void countSimulatedTick(PathSearchStatistics* statistics)
        {
            if (statistics != nullptr)
            {
                ++statistics->simulatedTicks;
            }
        }

        void requireStep(float stepSeconds)
        {
            if (!isFinitePositive(stepSeconds))
            {
                throw std::invalid_argument(
                    "Navigation simulation step must be finite and positive");
            }
        }

        bool sameIntentions(const InputIntentions& first, const InputIntentions& second)
        {
            return first.direction == second.direction &&
                   first.aimDirection == second.aimDirection &&
                   first.jumpPressed == second.jumpPressed && first.jumpHeld == second.jumpHeld &&
                   first.primaryAttackPressed == second.primaryAttackPressed;
        }

        // Extends the last step while the intentions hold, so a program is a few long
        // steps rather than one per tick.
        void recordSimulationInput(
            InputProgram& program,
            const InputIntentions& intentions,
            float stepSeconds)
        {
            if (!program.empty() && sameIntentions(program.back().intentions, intentions))
            {
                program.back().duration += stepSeconds;
                return;
            }
            program.push_back({stepSeconds, intentions});
        }

        // Grows the footprint to the cells around the bounds, one tile out on every side,
        // since collision and support read the tiles beside the body as well as under it.
        void sweep(CellRange& footprint, int tileSize, const Aabb& bounds)
        {
            const glm::vec2 margin{static_cast<float>(tileSize), static_cast<float>(tileSize)};
            const Aabb around{bounds.position - margin, bounds.size + 2.0F * margin};
            footprint = unionOf(footprint, cellsCovered(tileSize, around));
        }

        // No cell the bounds cover blocks movement.
        bool bodyFits(const TileMap& map, const Aabb& bounds)
        {
            const CellRange cells = cellsCovered(map.tileSize(), bounds);
            for (int row = cells.first.y; row <= cells.last.y; ++row)
            {
                for (int column = cells.first.x; column <= cells.last.x; ++column)
                {
                    if (map.blocksMovement({column, row}))
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        // Simulates a complete start-to-stop walk using the real path follower, movement,
        // and collision code. Returns its fixed-update cost, or nullopt when the actor
        // cannot reach and stop at the destination within the connection simulation limit.
        std::optional<int> trySimulateWalkCost(
            const TileMap& map,
            GridPosition start,
            GridPosition destinationCell,
            glm::vec2 bodySize,
            const PlatformerMovementConfig& config,
            float stepSeconds,
            PathSearchStatistics* statistics,
            CellRange& footprint)
        {
            Body body{boxInCell(map.tileSize(), start, bodySize), {0.0F, 0.0F}};
            PlatformerMovement movement{config, true, 0.0F, 0.0F};
            PathFollower follower;
            setPath(follower, {start, {{destinationCell, Traversal::Walk, {}}}}, destinationCell);

            for (int tick = 0; tick < MaximumConnectionSimulationTicks; ++tick)
            {
                const InputIntentions intentions =
                    followPlatformerPath(map.tileSize(), body, movement, follower, stepSeconds);
                if (pathComplete(follower))
                {
                    return tick;
                }
                updatePlatformerMovement(map, body, movement, intentions, stepSeconds);
                sweep(footprint, map.tileSize(), body.bounds);
                countSimulatedTick(statistics);
            }
            return std::nullopt;
        }

        // Whether the bounds have reached the map's edge in the direction travelled, past
        // which a traversal cannot go.
        bool touchesHorizontalMapEdge(const TileMap& map, const Aabb& bounds, float direction)
        {
            return (direction < 0.0F && bounds.position.x <= EdgeTolerance) ||
                   (direction > 0.0F &&
                    bounds.position.x + bounds.size.x >= map.pixelWidth() - EdgeTolerance);
        }

        // The inputs of a fall or a jump at this tick: pushing one way until landed, and
        // for a jump, pressing on the first tick and holding for as many as asked.
        InputIntentions makeTraversalIntentions(
            Traversal traversal,
            float direction,
            int tick,
            int jumpHoldTicks,
            bool hasLanded)
        {
            InputIntentions intentions;
            intentions.direction.x = hasLanded ? 0.0F : direction;
            if (traversal == Traversal::Jump && !hasLanded)
            {
                intentions.jumpPressed = tick == 0;
                intentions.jumpHeld = tick < jumpHoldTicks;
            }
            return intentions;
        }

        // The standable cell under the feet, unless it is the cell the traversal left.
        std::optional<GridPosition> tryFindLandingCell(
            const TileMap& map,
            GridPosition start,
            const Aabb& bounds,
            glm::vec2 bodySize)
        {
            const GridPosition destinationCell = cellAtFeet(map.tileSize(), feetOf(bounds));
            if (destinationCell == start || !canStandAt(map, destinationCell, bodySize))
            {
                return std::nullopt;
            }
            return destinationCell;
        }

        // Simulates leaving the ground, landing on another standable cell, and braking
        // to a stop. Returns the connection and its recorded inputs, or nullopt when the
        // traversal cannot complete within the connection simulation limit.
        std::optional<NavigationNeighbor> trySimulateAirborneConnection(
            const TileMap& map,
            GridPosition start,
            glm::vec2 bodySize,
            const PlatformerMovementConfig& config,
            Traversal traversal,
            float direction,
            int jumpHoldTicks,
            float stepSeconds,
            PathSearchStatistics* statistics,
            CellRange& footprint)
        {
            Body body{boxInCell(map.tileSize(), start, bodySize), {0.0F, 0.0F}};
            PlatformerMovement movement{config, true, 0.0F, 0.0F};
            InputProgram program;
            bool leftGround = false;
            std::optional<GridPosition> landing;

            for (int tick = 0; tick < MaximumConnectionSimulationTicks; ++tick)
            {
                if (touchesHorizontalMapEdge(map, body.bounds, direction))
                {
                    return std::nullopt;
                }

                const InputIntentions intentions = makeTraversalIntentions(
                    traversal, direction, tick, jumpHoldTicks, landing.has_value());
                recordSimulationInput(program, intentions, stepSeconds);
                updatePlatformerMovement(map, body, movement, intentions, stepSeconds);
                sweep(footprint, map.tileSize(), body.bounds);
                countSimulatedTick(statistics);

                leftGround = leftGround || !movement.grounded;
                if (!leftGround || !movement.grounded)
                {
                    continue;
                }

                if (!landing.has_value())
                {
                    landing = tryFindLandingCell(map, start, body.bounds, bodySize);
                    if (!landing.has_value())
                    {
                        return std::nullopt;
                    }
                }
                if (body.velocity.x != 0.0F)
                {
                    continue;
                }
                const GridPosition stoppedCell = cellAtFeet(map.tileSize(), feetOf(body.bounds));
                if (stoppedCell != landing.value())
                {
                    return std::nullopt;
                }
                const int ticks = tick + 1;
                return NavigationNeighbor{stoppedCell, traversal, ticks, program};
            }
            return std::nullopt;
        }

        // Adds the connection, or replaces the one already found to the same cell by the
        // same traversal when this one is cheaper.
        void keepCheapest(std::vector<NavigationNeighbor>& neighbors, NavigationNeighbor candidate)
        {
            const auto existing = std::find_if(
                neighbors.begin(),
                neighbors.end(),
                [&candidate](const NavigationNeighbor& neighbor)
                {
                    return neighbor.destinationCell == candidate.destinationCell &&
                           neighbor.traversal == candidate.traversal;
                });
            if (existing == neighbors.end())
            {
                neighbors.push_back(std::move(candidate));
            }
            else if (candidate.cost < existing->cost)
            {
                *existing = std::move(candidate);
            }
        }
    }

    int platformerTickHeuristic(
        int tileSize,
        GridPosition cell,
        GridPosition goal,
        const PlatformerMovementConfig& movement,
        float stepSeconds)
    {
        requireStep(stepSeconds);
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
        // connections come from connectionsOf, called with the cell and a visitor to hand
        // each one to; a jump is charged its cost and the start penalty. With reached, the
        // cells expanded are collected there. The plain search hands it connections
        // simulated for this search alone; the cached search hands it the cache's.
        template <typename ConnectionSource>
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
            // Call the overload without a heuristic to compare A* with a plain lowest-cost
            // search.
            return findLowestCostPath(
                start, goal, map.size(), neighbors, heuristic, statistics, reached);
        }

        // The search with a cache: answered from what the cache remembers when it can,
        // run over the cache's connections otherwise, and what it learns kept for the next.
        std::optional<NavigationPath> findCachedPlatformerPath(
            const TileMap& map,
            GridPosition start,
            GridPosition goal,
            glm::vec2 bodySize,
            const PlatformerMovementConfig& movement,
            float stepSeconds,
            const PlatformerNavigationConfig& navigation,
            PathSearchStatistics* statistics,
            PlatformerConnectionCache& cache)
        {
            cache.syncWith(map);
            const ConnectionBody body{bodySize, movement, stepSeconds};
            const PathQuery query{start, goal, navigation.jumpStartPenaltyTicks};
            // A search answered before: while the connections hold, so does the cheapest
            // route between two cells for one penalty.
            const NavigationPath* kept = cache.pathKept(query, body);
            if (kept != nullptr)
            {
                if (statistics != nullptr)
                {
                    ++statistics->pathsRemembered;
                }
                return *kept;
            }
            // A search that failed from this start has already found every cell it leads
            // to, so a goal outside them has no path and there is nothing to search.
            const std::vector<GridPosition>* reachable = cache.reachableFrom(start, body);
            if (reachable != nullptr &&
                std::find(reachable->begin(), reachable->end(), goal) == reachable->end())
            {
                return std::nullopt;
            }

            // The connections are read where the cache keeps them. A cell a break dropped
            // waits for the refill rather than being simulated here, so the search goes on
            // without its connections.
            bool incomplete = false;
            const auto connectionsOf = [&](GridPosition cell, const auto& visit)
            {
                if (cache.isPending(cell, body))
                {
                    cache.prioritise(cell, body);
                    incomplete = true;
                    return;
                }
                for (const NavigationNeighbor& neighbor : platformerNeighborsKept(
                         map, cell, bodySize, movement, stepSeconds, cache, statistics))
                {
                    visit(neighbor);
                }
            };
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
                &reached);

            // A search that went without some cell's connections has learned nothing the
            // cache may keep: a path it found still leads to the goal, but no path means
            // the caller asks again once the refill has caught up.
            if (incomplete)
            {
                if (!path.has_value() && statistics != nullptr)
                {
                    ++statistics->deferred;
                }
                return path;
            }
            // What a failed search learns is kept; a found path too.
            if (path.has_value())
            {
                cache.keepPath(query, body, *path);
            }
            else
            {
                cache.keepReachable(start, body, std::move(reached));
            }
            return path;
        }
    }

    std::optional<NavigationPath> findPlatformerPath(
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
        requireStep(stepSeconds);
        if (navigation.jumpStartPenaltyTicks < 0)
        {
            throw std::invalid_argument("A jump start penalty cannot be negative");
        }
        // A goal off the map has no path, and there is nothing to learn from searching.
        if (!map.contains(start) || !map.contains(goal))
        {
            return std::nullopt;
        }
        if (cache != nullptr)
        {
            return findCachedPlatformerPath(
                map, start, goal, bodySize, movement, stepSeconds, navigation, statistics, *cache);
        }
        // Without a cache, every cell's connections are simulated for this search alone.
        const auto connectionsOf = [&](GridPosition cell, const auto& visit)
        {
            for (const NavigationNeighbor& neighbor :
                 platformerNeighbors(map, cell, bodySize, movement, stepSeconds, statistics))
            {
                visit(neighbor);
            }
        };
        return searchPlatformerPath(
            map,
            start,
            goal,
            movement,
            stepSeconds,
            navigation,
            connectionsOf,
            statistics,
            nullptr);
    }

    bool canStandAt(const TileMap& map, GridPosition cell, glm::vec2 bodySize)
    {
        if (!isFinite(bodySize) || bodySize.x <= 0.0F || bodySize.y <= 0.0F)
        {
            throw std::invalid_argument("Navigation body size must be finite and positive");
        }
        return map.contains(cell) && !map.blocksMovement(cell) &&
               map.blocksMovement({cell.x, cell.y + 1}) &&
               bodyFits(map, boxInCell(map.tileSize(), cell, bodySize));
    }

    std::optional<GridPosition> findPlatformerStartCell(const TileMap& map, const Aabb& bounds)
    {
        if (!isFinite(bounds.position) || !isFinite(bounds.size) || bounds.size.x <= 0.0F ||
            bounds.size.y <= 0.0F)
        {
            throw std::invalid_argument(
                "A platformer navigation body must have finite, positive-sized bounds");
        }

        const glm::vec2 feet = feetOf(bounds);
        const GridPosition feetCell = cellAtFeet(map.tileSize(), feet);
        if (canStandAt(map, feetCell, bounds.size))
        {
            return feetCell;
        }

        const CellRange cells = cellsCovered(map.tileSize(), bounds);
        std::optional<GridPosition> closest;
        float closestDistance = 0.0F;
        for (int column = cells.first.x; column <= cells.last.x; ++column)
        {
            const GridPosition candidate{column, feetCell.y};
            if (!canStandAt(map, candidate, bounds.size))
            {
                continue;
            }

            const float distance = std::abs(feetInCell(map.tileSize(), candidate).x - feet.x);
            if (!closest.has_value() || distance < closestDistance)
            {
                closest = candidate;
                closestDistance = distance;
            }
        }
        return closest;
    }

    std::optional<GridPosition> findPlatformerChaseCell(
        const TileMap& map,
        glm::vec2 lastSeenFeet,
        glm::vec2 bodySize)
    {
        if (!isFinite(lastSeenFeet) || !isFinite(bodySize) || bodySize.x <= 0.0F ||
            bodySize.y <= 0.0F)
        {
            throw std::invalid_argument(
                "A chase destination requires finite feet and a finite, positive body size");
        }

        // Avoid converting an out-of-map world position to an integer grid cell.
        if (lastSeenFeet.x >= 0.0F && lastSeenFeet.x < map.pixelWidth() && lastSeenFeet.y >= 0.0F &&
            lastSeenFeet.y <= map.pixelHeight())
        {
            const GridPosition targetCell = cellAtFeet(map.tileSize(), lastSeenFeet);
            if (canStandAt(map, targetCell, bodySize))
            {
                return targetCell;
            }
        }

        // Nearest first: rings of cells around the cell nearest the feet, out until a ring
        // can no longer beat the best found. Every cell in ring r lies at least r - 1
        // tiles from the feet, so once that exceeds the best distance the rest of the map
        // cannot win. The closest feet position wins; equal distances keep row, then
        // column order, as a scan of the whole map would.
        const auto tileSize = static_cast<float>(map.tileSize());
        const GridPosition anchor{
            static_cast<int>(
                std::floor(std::clamp(lastSeenFeet.x, 0.0F, map.pixelWidth() - 1.0F) / tileSize)),
            static_cast<int>(
                std::floor(std::clamp(lastSeenFeet.y, 0.0F, map.pixelHeight() - 1.0F) / tileSize))};
        const int farthestRing =
            std::max({anchor.x, map.width() - 1 - anchor.x, anchor.y, map.height() - 1 - anchor.y});

        std::optional<GridPosition> closest;
        double closestDistanceSquared = 0.0;
        const auto consider = [&](GridPosition candidate)
        {
            if (!map.contains(candidate) || !canStandAt(map, candidate, bodySize))
            {
                return;
            }
            const glm::vec2 candidateFeet = feetInCell(map.tileSize(), candidate);
            const double dx = static_cast<double>(candidateFeet.x) - lastSeenFeet.x;
            const double dy = static_cast<double>(candidateFeet.y) - lastSeenFeet.y;
            const double distanceSquared = dx * dx + dy * dy;
            const bool earlierInScanOrder =
                closest.has_value() && (candidate.y < closest->y ||
                                        (candidate.y == closest->y && candidate.x < closest->x));
            if (!closest.has_value() || distanceSquared < closestDistanceSquared ||
                (distanceSquared == closestDistanceSquared && earlierInScanOrder))
            {
                closest = candidate;
                closestDistanceSquared = distanceSquared;
            }
        };

        for (int ring = 0; ring <= farthestRing; ++ring)
        {
            if (closest.has_value())
            {
                const double nearestPossible = static_cast<double>(ring - 1) * tileSize;
                if (nearestPossible > 0.0 &&
                    nearestPossible * nearestPossible > closestDistanceSquared)
                {
                    break;
                }
            }
            if (ring == 0)
            {
                consider(anchor);
                continue;
            }
            for (int column = anchor.x - ring; column <= anchor.x + ring; ++column)
            {
                consider({column, anchor.y - ring});
                consider({column, anchor.y + ring});
            }
            for (int row = anchor.y - ring + 1; row <= anchor.y + ring - 1; ++row)
            {
                consider({anchor.x - ring, row});
                consider({anchor.x + ring, row});
            }
        }
        return closest;
    }

    namespace
    {
        struct SimulatedConnections
        {
            std::vector<NavigationNeighbor> connections;
            // Every cell the simulations swept or read, as one rectangle.
            CellRange footprint;
        };

        // Every connection leaving a cell, simulated with the real movement code, with the
        // footprint of the cells that decided them. A cell that cannot be stood on has
        // no connections, and a footprint of itself and its surroundings.
        SimulatedConnections simulatePlatformerNeighbors(
            const TileMap& map,
            GridPosition cell,
            glm::vec2 bodySize,
            const PlatformerMovementConfig& movement,
            float stepSeconds,
            PathSearchStatistics* statistics)
        {
            const int tileSize = map.tileSize();
            SimulatedConnections result{
                {}, cellsCovered(tileSize, boxInCell(tileSize, cell, bodySize))};
            std::vector<NavigationNeighbor>& neighbors = result.connections;
            CellRange& footprint = result.footprint;
            // Standing reads the cell, what it covers and what is under it.
            const auto standable = [&](GridPosition candidate)
            {
                sweep(footprint, tileSize, boxInCell(tileSize, candidate, bodySize));
                return canStandAt(map, candidate, bodySize);
            };
            if (!standable(cell))
            {
                return result;
            }

            constexpr std::array<int, 2> Directions{-1, 1};
            for (const int direction : Directions)
            {
                const GridPosition adjacent{cell.x + direction, cell.y};
                if (standable(adjacent))
                {
                    GridPosition walkDestination = adjacent;
                    while (standable(walkDestination))
                    {
                        const std::optional<int> walkCost = trySimulateWalkCost(
                            map,
                            cell,
                            walkDestination,
                            bodySize,
                            movement,
                            stepSeconds,
                            statistics,
                            footprint);
                        if (!walkCost.has_value())
                        {
                            // Destinations are checked nearest first. Once a continuous walk
                            // exceeds the simulation limit, farther destinations are excluded.
                            break;
                        }

                        neighbors.push_back(
                            {walkDestination, Traversal::Walk, walkCost.value_or(1), {}});
                        walkDestination.x += direction;
                    }
                }
                else
                {
                    const std::optional<NavigationNeighbor> fall = trySimulateAirborneConnection(
                        map,
                        cell,
                        bodySize,
                        movement,
                        Traversal::Fall,
                        static_cast<float>(direction),
                        0,
                        stepSeconds,
                        statistics,
                        footprint);
                    if (fall.has_value())
                    {
                        keepCheapest(neighbors, fall.value());
                    }
                }

                constexpr std::array<int, 2> JumpHoldTicks{1, MaximumConnectionSimulationTicks};
                for (const int holdTicks : JumpHoldTicks)
                {
                    const std::optional<NavigationNeighbor> jump = trySimulateAirborneConnection(
                        map,
                        cell,
                        bodySize,
                        movement,
                        Traversal::Jump,
                        static_cast<float>(direction),
                        holdTicks,
                        stepSeconds,
                        statistics,
                        footprint);
                    if (jump.has_value())
                    {
                        keepCheapest(neighbors, jump.value());
                    }
                }
            }
            return result;
        }
    }

    RefillWork refillPlatformerConnections(
        const TileMap& map,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement,
        float stepSeconds,
        PlatformerConnectionCache& cache,
        int tickBudget)
    {
        requireStep(stepSeconds);
        if (tickBudget < 0)
        {
            throw std::invalid_argument("A refill budget cannot be negative");
        }
        cache.syncWith(map);
        const ConnectionBody body{bodySize, movement, stepSeconds};
        RefillWork work;
        while (work.simulatedTicks < tickBudget)
        {
            const std::optional<GridPosition> next = cache.nextPending(body);
            if (!next.has_value())
            {
                break;
            }
            PathSearchStatistics statistics;
            platformerNeighborsKept(
                map,
                next.value_or(GridPosition{}),
                bodySize,
                movement,
                stepSeconds,
                cache,
                &statistics);
            ++work.cells;
            work.simulatedTicks += statistics.simulatedTicks;
        }
        return work;
    }

    void keepAllPlatformerConnections(
        const TileMap& map,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement,
        float stepSeconds,
        PlatformerConnectionCache& cache)
    {
        cache.syncWith(map);
        for (int row = 0; row < map.height(); ++row)
        {
            for (int column = 0; column < map.width(); ++column)
            {
                platformerNeighborsKept(map, {column, row}, bodySize, movement, stepSeconds, cache);
            }
        }
    }

    const std::vector<NavigationNeighbor>& platformerNeighborsKept(
        const TileMap& map,
        GridPosition cell,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement,
        float stepSeconds,
        PlatformerConnectionCache& cache,
        PathSearchStatistics* statistics)
    {
        requireStep(stepSeconds);
        cache.syncWith(map);
        const ConnectionBody body{bodySize, movement, stepSeconds};
        const std::vector<NavigationNeighbor>* kept = cache.find(cell, body);
        if (kept != nullptr)
        {
            if (statistics != nullptr)
            {
                ++statistics->cellsReused;
            }
            return *kept;
        }
        SimulatedConnections simulated =
            simulatePlatformerNeighbors(map, cell, bodySize, movement, stepSeconds, statistics);
        return cache.keep(cell, body, std::move(simulated.connections), simulated.footprint);
    }

    std::vector<NavigationNeighbor> platformerNeighbors(
        const TileMap& map,
        GridPosition cell,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement,
        float stepSeconds,
        PathSearchStatistics* statistics,
        PlatformerConnectionCache* cache)
    {
        requireStep(stepSeconds);
        if (cache != nullptr)
        {
            return platformerNeighborsKept(
                map, cell, bodySize, movement, stepSeconds, *cache, statistics);
        }
        return simulatePlatformerNeighbors(map, cell, bodySize, movement, stepSeconds, statistics)
            .connections;
    }
}
