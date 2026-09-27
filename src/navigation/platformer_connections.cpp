#include "simple_platformer/navigation/platformer_connections.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/input/input_program.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/connection_cache.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/path_search.hpp"
#include "simple_platformer/navigation/platformer_cells.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    namespace
    {
        // Limit a traversal to 120 simulated updates (two seconds at 60 Hz).
        // It must land and stop within that budget to become a connection.
        constexpr int MaximumConnectionSimulationTicks = 120;

        void countSimulatedTick(PathSearchStatistics* statistics)
        {
            if (statistics != nullptr)
            {
                ++statistics->simulatedTicks;
            }
        }

        // Airborne simulation leaves ledge avoidance and contact damage off, so only its
        // recorded intention fields need comparing when consecutive ticks are merged.
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

        // Simulates a complete start-to-stop walk using the real path follower, movement,
        // and collision code. Returns its fixed-update cost, or no cost when the actor
        // cannot reach and stop at the destination within the connection simulation
        // limit, with the cells it swept as offsets from the start.
        RememberedWalk simulateWalk(
            const TileMap& map,
            GridPosition start,
            GridPosition destinationCell,
            glm::vec2 bodySize,
            const PlatformerMovementConfig& config,
            float stepSeconds,
            PathSearchStatistics* statistics)
        {
            const int tileSize = map.tileSize();
            Body body{boxInCell(tileSize, start, bodySize), {0.0F, 0.0F}};
            PlatformerMovement movement{config, true, 0.0F, 0.0F};
            PathFollower follower;
            setPath(follower, {start, {{destinationCell, Traversal::Walk, {}}}}, destinationCell);

            RememberedWalk walk{std::nullopt, cellsCovered(tileSize, body.bounds)};
            for (int tick = 0; tick < MaximumConnectionSimulationTicks; ++tick)
            {
                const InputIntentions intentions =
                    followPlatformerPath(tileSize, body, movement, follower, stepSeconds);
                if (pathComplete(follower))
                {
                    walk.cost = tick;
                    break;
                }
                updatePlatformerMovement(map, body, movement, intentions, stepSeconds);
                sweep(walk.sweep, tileSize, body.bounds);
                countSimulatedTick(statistics);
            }
            walk.sweep = {
                {walk.sweep.first.x - start.x, walk.sweep.first.y - start.y},
                {walk.sweep.last.x - start.x, walk.sweep.last.y - start.y}};
            return walk;
        }

        // The walk from a cell to another along its floor. A walk starts and ends at rest
        // on flat ground, so its cost and sweep depend on the distance and the body alone:
        // with a cache, each distance is simulated once and remembered.
        RememberedWalk walkBetween(
            const TileMap& map,
            GridPosition start,
            GridPosition destinationCell,
            glm::vec2 bodySize,
            const PlatformerMovementConfig& config,
            float stepSeconds,
            PathSearchStatistics* statistics,
            PlatformerConnectionCache* cache)
        {
            const int columns = destinationCell.x - start.x;
            const ConnectionBody body{bodySize, config, stepSeconds};
            if (cache != nullptr)
            {
                const RememberedWalk* remembered = cache->walkKept(columns, body);
                if (remembered != nullptr)
                {
                    return *remembered;
                }
            }
            const RememberedWalk walk = simulateWalk(
                map, start, destinationCell, bodySize, config, stepSeconds, statistics);
            if (cache != nullptr)
            {
                cache->keepWalk(columns, body, walk);
            }
            return walk;
        }

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

        struct SimulatedConnections
        {
            std::vector<NavigationNeighbor> connections;
            // Every cell the simulations swept or read, as one rectangle.
            CellRange footprint;
        };

        // Every connection leaving a cell, simulated with the real movement code, with the
        // footprint of the cells that decided them. A cell that cannot be stood on has
        // no connections, and a footprint of itself and its surroundings. With a cache,
        // walks are remembered per distance rather than simulated again.
        SimulatedConnections simulatePlatformerNeighbors(
            const TileMap& map,
            GridPosition cell,
            glm::vec2 bodySize,
            const PlatformerMovementConfig& movement,
            float stepSeconds,
            PathSearchStatistics* statistics,
            PlatformerConnectionCache* cache)
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
                        const RememberedWalk walk = walkBetween(
                            map,
                            cell,
                            walkDestination,
                            bodySize,
                            movement,
                            stepSeconds,
                            statistics,
                            cache);
                        footprint = unionOf(
                            footprint,
                            {{cell.x + walk.sweep.first.x, cell.y + walk.sweep.first.y},
                             {cell.x + walk.sweep.last.x, cell.y + walk.sweep.last.y}});
                        if (!walk.cost.has_value())
                        {
                            // Destinations are checked nearest first. Once a continuous walk
                            // exceeds the simulation limit, farther destinations are excluded.
                            break;
                        }

                        neighbors.push_back(
                            {walkDestination, Traversal::Walk, walk.cost.value_or(1), {}});
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

    const std::vector<NavigationNeighbor>& platformerNeighborsKept(
        const TileMap& map,
        GridPosition cell,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement,
        float stepSeconds,
        PlatformerConnectionCache& cache,
        PathSearchStatistics* statistics)
    {
        requirePositiveSeconds(stepSeconds, "Navigation simulation step");
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
        SimulatedConnections simulated = simulatePlatformerNeighbors(
            map, cell, bodySize, movement, stepSeconds, statistics, &cache);
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
        requirePositiveSeconds(stepSeconds, "Navigation simulation step");
        if (cache != nullptr)
        {
            return platformerNeighborsKept(
                map, cell, bodySize, movement, stepSeconds, *cache, statistics);
        }
        return simulatePlatformerNeighbors(
                   map, cell, bodySize, movement, stepSeconds, statistics, nullptr)
            .connections;
    }
}
