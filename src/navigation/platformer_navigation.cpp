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
#include "simple_platformer/navigation/input_program.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/path_search.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/timing/fixed_step.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    namespace
    {
        constexpr float SimulationStep = static_cast<float>(FixedDeltaSeconds);
        constexpr int MaximumConnectionSimulationTicks = 120;

        bool sameIntentions(const InputIntentions& first, const InputIntentions& second)
        {
            return first.direction == second.direction &&
                   first.aimDirection == second.aimDirection &&
                   first.jumpPressed == second.jumpPressed && first.jumpHeld == second.jumpHeld &&
                   first.primaryAttackPressed == second.primaryAttackPressed;
        }

        void recordSimulationInput(InputProgram& program, const InputIntentions& intentions)
        {
            if (!program.empty() && sameIntentions(program.back().intentions, intentions))
            {
                program.back().duration += SimulationStep;
                return;
            }
            program.push_back({SimulationStep, intentions});
        }

        bool bodyFits(const TileMap& map, const Aabb& bounds)
        {
            constexpr float Inside = 0.001F;
            const float tileSize = static_cast<float>(TileSize);
            const int firstColumn =
                static_cast<int>(std::floor((bounds.position.x + Inside) / tileSize));
            const int lastColumn = static_cast<int>(
                std::floor((bounds.position.x + bounds.size.x - Inside) / tileSize));
            const int firstRow =
                static_cast<int>(std::floor((bounds.position.y + Inside) / tileSize));
            const int lastRow = static_cast<int>(
                std::floor((bounds.position.y + bounds.size.y - Inside) / tileSize));

            for (int row = firstRow; row <= lastRow; ++row)
            {
                for (int column = firstColumn; column <= lastColumn; ++column)
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
            GridPosition destination,
            glm::vec2 bodySize,
            const PlatformerMovementConfig& config)
        {
            Body body{boxInCell(start, bodySize), {0.0F, 0.0F}};
            PlatformerMovement movement{config, true, 0.0F, 0.0F};
            Facing facing = destination.x < start.x ? Facing::Left : Facing::Right;
            PathFollower follower;
            setPath(follower, {start, {{destination, Traversal::Walk, {}}}}, destination);

            for (int tick = 0; tick < MaximumConnectionSimulationTicks; ++tick)
            {
                const InputIntentions intentions =
                    followPlatformerPath(body, movement, follower, SimulationStep);
                if (pathComplete(follower))
                {
                    return tick;
                }
                updatePlatformerMovement(map, body, movement, intentions, facing, SimulationStep);
            }
            return std::nullopt;
        }

        bool touchesHorizontalMapEdge(const TileMap& map, const Aabb& bounds, float direction)
        {
            constexpr float WallTolerance = 0.001F;
            return (direction < 0.0F && bounds.position.x <= WallTolerance) ||
                   (direction > 0.0F &&
                    bounds.position.x + bounds.size.x >= map.pixelWidth() - WallTolerance);
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
            const GridPosition destination = cellAtFeet(feetOf(bounds));
            if (destination == start || !canStandAt(map, destination, bodySize))
            {
                return std::nullopt;
            }
            return destination;
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
            int jumpHoldTicks)
        {
            Body body{boxInCell(start, bodySize), {0.0F, 0.0F}};
            PlatformerMovement movement{config, true, 0.0F, 0.0F};
            Facing facing = direction < 0.0F ? Facing::Left : Facing::Right;
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
                recordSimulationInput(program, intentions);
                updatePlatformerMovement(map, body, movement, intentions, facing, SimulationStep);

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
                const GridPosition stoppedCell = cellAtFeet(feetOf(body.bounds));
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
                    return neighbor.destination == candidate.destination &&
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
        GridPosition position,
        GridPosition goal,
        const PlatformerMovementConfig& movement)
    {
        if (!std::isfinite(movement.maximumSpeed) || movement.maximumSpeed < 0.0F)
        {
            throw std::invalid_argument(
                "Platformer navigation maximum speed must be finite and non-negative");
        }

        const int columnDistance = std::abs(goal.x - position.x);
        if (columnDistance == 0 || movement.maximumSpeed == 0.0F)
        {
            return 0;
        }

        // Reaching any point inside the goal column is sufficient. Ignoring acceleration,
        // braking, obstacles, and vertical travel keeps this estimate optimistic.
        const float minimumDistance =
            (static_cast<float>(columnDistance) - 0.5F) * static_cast<float>(TileSize);
        const float maximumDistancePerTick = movement.maximumSpeed * SimulationStep;
        return static_cast<int>(std::ceil(minimumDistance / maximumDistancePerTick));
    }

    std::optional<NavigationPath> findPlatformerPath(
        const TileMap& map,
        GridPosition start,
        GridPosition goal,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement,
        const PlatformerNavigationConfig& navigation)
    {
        if (navigation.jumpStartPenaltyTicks < 0)
        {
            throw std::invalid_argument("A jump start penalty cannot be negative");
        }
        const GridNeighborFunction neighbors =
            [&map, bodySize, &movement, &navigation](GridPosition position)
        {
            std::vector<NavigationNeighbor> result =
                platformerNeighbors(map, position, bodySize, movement);
            for (NavigationNeighbor& neighbor : result)
            {
                if (neighbor.traversal != Traversal::Jump)
                {
                    continue;
                }
                if (neighbor.cost >
                    std::numeric_limits<int>::max() - navigation.jumpStartPenaltyTicks)
                {
                    throw std::overflow_error("A navigation connection cost is too large");
                }
                neighbor.cost += navigation.jumpStartPenaltyTicks;
            }
            return result;
        };
        const GridHeuristicFunction heuristic =
            [&movement](GridPosition position, GridPosition goal)
        { return platformerTickHeuristic(position, goal, movement); };

        // Remove the final argument to compare A* with the default Dijkstra search.
        return findLowestCostPath(start, goal, neighbors, heuristic);
    }

    bool canStandAt(const TileMap& map, GridPosition position, glm::vec2 bodySize)
    {
        if (!isFinite(bodySize) || bodySize.x <= 0.0F || bodySize.y <= 0.0F)
        {
            throw std::invalid_argument("Navigation body size must be finite and positive");
        }
        return map.contains(position) && !map.blocksMovement(position) &&
               map.blocksMovement({position.x, position.y + 1}) &&
               bodyFits(map, boxInCell(position, bodySize));
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
        const GridPosition feetCell = cellAtFeet(feet);
        if (canStandAt(map, feetCell, bounds.size))
        {
            return feetCell;
        }

        constexpr float Inside = 0.001F;
        const float tileSize = static_cast<float>(TileSize);
        const int firstColumn =
            static_cast<int>(std::floor((bounds.position.x + Inside) / tileSize));
        const int lastColumn =
            static_cast<int>(std::floor((bounds.position.x + bounds.size.x - Inside) / tileSize));
        std::optional<GridPosition> closest;
        float closestDistance = 0.0F;
        for (int column = firstColumn; column <= lastColumn; ++column)
        {
            const GridPosition candidate{column, feetCell.y};
            if (!canStandAt(map, candidate, bounds.size))
            {
                continue;
            }

            const float distance = std::abs(feetInCell(candidate).x - feet.x);
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
            const GridPosition targetCell = cellAtFeet(lastSeenFeet);
            if (canStandAt(map, targetCell, bodySize))
            {
                return targetCell;
            }
        }

        std::optional<GridPosition> closest;
        double closestDistanceSquared = 0.0;
        for (int row = 0; row < map.height(); ++row)
        {
            for (int column = 0; column < map.width(); ++column)
            {
                const GridPosition candidate{column, row};
                if (!canStandAt(map, candidate, bodySize))
                {
                    continue;
                }

                const glm::vec2 candidateFeet = feetInCell(candidate);
                const double dx = static_cast<double>(candidateFeet.x) - lastSeenFeet.x;
                const double dy = static_cast<double>(candidateFeet.y) - lastSeenFeet.y;
                const double distanceSquared = dx * dx + dy * dy;
                if (!closest.has_value() || distanceSquared < closestDistanceSquared)
                {
                    closest = candidate;
                    closestDistanceSquared = distanceSquared;
                }
            }
        }
        return closest;
    }

    std::vector<NavigationNeighbor> platformerNeighbors(
        const TileMap& map,
        GridPosition position,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement)
    {
        if (!canStandAt(map, position, bodySize))
        {
            return {};
        }

        std::vector<NavigationNeighbor> neighbors;
        constexpr std::array<int, 2> Directions{-1, 1};
        for (const int direction : Directions)
        {
            const GridPosition adjacent{position.x + direction, position.y};
            if (canStandAt(map, adjacent, bodySize))
            {
                GridPosition walkDestination = adjacent;
                while (canStandAt(map, walkDestination, bodySize))
                {
                    const std::optional<int> walkCost =
                        trySimulateWalkCost(map, position, walkDestination, bodySize, movement);
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
                    position,
                    bodySize,
                    movement,
                    Traversal::Fall,
                    static_cast<float>(direction),
                    0);
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
                    position,
                    bodySize,
                    movement,
                    Traversal::Jump,
                    static_cast<float>(direction),
                    holdTicks);
                if (jump.has_value())
                {
                    keepCheapest(neighbors, jump.value());
                }
            }
        }
        return neighbors;
    }
}
