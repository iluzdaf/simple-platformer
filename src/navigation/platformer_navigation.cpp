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
            PathSearchStatistics* statistics)
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
                countSimulatedTick(statistics);
            }
            return std::nullopt;
        }

        bool touchesHorizontalMapEdge(const TileMap& map, const Aabb& bounds, float direction)
        {
            return (direction < 0.0F && bounds.position.x <= EdgeTolerance) ||
                   (direction > 0.0F &&
                    bounds.position.x + bounds.size.x >= map.pixelWidth() - EdgeTolerance);
        }

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
            PathSearchStatistics* statistics)
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
        // With a cache, the connections are read where the cache keeps them; without one
        // they are simulated for this search alone. Either way a jump is charged its cost
        // and the start penalty.
        const GridNeighborFunction neighbors =
            [&map, bodySize, &movement, stepSeconds, &navigation, statistics, cache](
                GridPosition cell, const GridNeighborVisitor& visit)
        {
            const auto charge = [&](const NavigationNeighbor& neighbor)
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
            };
            if (cache != nullptr)
            {
                for (const NavigationNeighbor& neighbor : platformerNeighborsKept(
                         map, cell, bodySize, movement, stepSeconds, *cache, statistics))
                {
                    charge(neighbor);
                }
                return;
            }
            for (const NavigationNeighbor& neighbor :
                 platformerNeighbors(map, cell, bodySize, movement, stepSeconds, statistics))
            {
                charge(neighbor);
            }
        };
        const GridHeuristicFunction heuristic =
            [&map, &movement, stepSeconds](GridPosition cell, GridPosition goal)
        { return platformerTickHeuristic(map.tileSize(), cell, goal, movement, stepSeconds); };

        // A search that failed from this start has already found every cell it leads to,
        // so a goal outside them has no path and there is nothing to search.
        const ConnectionBody body{bodySize, movement, stepSeconds};
        if (cache != nullptr)
        {
            const std::vector<GridPosition>* reachable = cache->reachableFrom(start, body);
            if (reachable != nullptr &&
                std::find(reachable->begin(), reachable->end(), goal) == reachable->end())
            {
                return std::nullopt;
            }
        }
        std::vector<GridPosition> reached;
        // Pass no heuristic to compare A* with the default Dijkstra search.
        std::optional<NavigationPath> path = findLowestCostPath(
            start, goal, neighbors, heuristic, statistics, cache != nullptr ? &reached : nullptr);
        if (!path.has_value() && cache != nullptr)
        {
            cache->keepReachable(start, body, std::move(reached));
        }
        return path;
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
        // Every connection leaving a cell, simulated with the real movement code. A cell
        // that cannot be stood on has none.
        std::vector<NavigationNeighbor> simulatePlatformerNeighbors(
            const TileMap& map,
            GridPosition cell,
            glm::vec2 bodySize,
            const PlatformerMovementConfig& movement,
            float stepSeconds,
            PathSearchStatistics* statistics)
        {
            std::vector<NavigationNeighbor> neighbors;
            if (!canStandAt(map, cell, bodySize))
            {
                return neighbors;
            }

            constexpr std::array<int, 2> Directions{-1, 1};
            for (const int direction : Directions)
            {
                const GridPosition adjacent{cell.x + direction, cell.y};
                if (canStandAt(map, adjacent, bodySize))
                {
                    GridPosition walkDestination = adjacent;
                    while (canStandAt(map, walkDestination, bodySize))
                    {
                        const std::optional<int> walkCost = trySimulateWalkCost(
                            map,
                            cell,
                            walkDestination,
                            bodySize,
                            movement,
                            stepSeconds,
                            statistics);
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
                        statistics);
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
                        statistics);
                    if (jump.has_value())
                    {
                        keepCheapest(neighbors, jump.value());
                    }
                }
            }
            return neighbors;
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
        cache.keep(
            cell,
            body,
            simulatePlatformerNeighbors(map, cell, bodySize, movement, stepSeconds, statistics));
        return *cache.find(cell, body);
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
        return simulatePlatformerNeighbors(map, cell, bodySize, movement, stepSeconds, statistics);
    }
}
