#include "simple_platformer/navigation/actor_navigation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/navigation/platformer_connection_cache.hpp"
#include "simple_platformer/navigation/route.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/route_search.hpp"
#include "simple_platformer/navigation/platformer_cells.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/navigation/traversal.hpp"
#include "simple_platformer/timing/frame_profile.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    namespace
    {
        // Added whenever a route starts a jump, so a small time saving does not make
        // a grounded actor hop unnecessarily.
        constexpr int JumpStartPenaltyTicks = 30;

        constexpr std::array<ClimbSurface, 4> Surfaces{
            ClimbSurface::None,
            ClimbSurface::LeftWall,
            ClimbSurface::RightWall,
            ClimbSurface::Ceiling};

        // In pixels: how far a body may sit off a surface and still rest on it.
        constexpr float RestingTolerance = 1.0F;

        // The location a body is resting at, judged from its bounds alone: among the places
        // the profile can rest near it, the one whose resting bounds match the body within
        // a pixel across its surface, and lie nearest along it, within a tile. Nothing
        // when the body rests nowhere, as when it is in the air.
        std::optional<RouteLocation> restingLocationOf(
            const TileMap& map,
            const Aabb& bounds,
            const PlatformerTraversalProfile& profile)
        {
            if (!isFinite(bounds.position))
            {
                throw std::invalid_argument("A resting body must have a finite position");
            }
            if (!isFinite(bounds.size) || bounds.size.x <= 0.0F || bounds.size.y <= 0.0F)
            {
                throw std::invalid_argument("Navigation body size must be finite and positive");
            }
            const int tileSize = map.tileSize();
            const CellRange covered = cellsCovered(tileSize, bounds);
            std::optional<RouteLocation> resting;
            float restingOffset = static_cast<float>(tileSize);
            for (int row = covered.first.y - 1; row <= covered.last.y + 1; ++row)
            {
                for (int column = covered.first.x - 1; column <= covered.last.x + 1; ++column)
                {
                    for (const ClimbSurface surface : Surfaces)
                    {
                        const RouteLocation candidate{{column, row}, surface};
                        if ((surface != ClimbSurface::None && !profile.climb.has_value()) ||
                            !canOccupy(map, candidate, bounds.size))
                        {
                            continue;
                        }
                        const glm::vec2 offset =
                            bounds.position -
                            boundsAtSurface(tileSize, candidate, bounds.size).position;
                        const bool onWall =
                            surface == ClimbSurface::LeftWall || surface == ClimbSurface::RightWall;
                        const float across = std::abs(onWall ? offset.x : offset.y);
                        const float along = std::abs(onWall ? offset.y : offset.x);
                        if (across <= RestingTolerance && along < restingOffset)
                        {
                            resting = candidate;
                            restingOffset = along;
                        }
                    }
                }
            }
            return resting;
        }

        // The path a route of locations gives: the body's resting feet at each.
        NavigationPath waypointsOf(int tileSize, const Route& route, glm::vec2 bodySize)
        {
            NavigationPath path{feetOf(boundsAtSurface(tileSize, route.start, bodySize)), {}};
            path.waypoints.reserve(route.steps.size());
            for (const RouteStep& step : route.steps)
            {
                path.waypoints.push_back(
                    {feetOf(boundsAtSurface(tileSize, step.destination, bodySize)),
                     step.traversal,
                     step.inputs});
            }
            return path;
        }

        // What a search's route gives its caller: Found when it ends in the goal cell,
        // Unreachable otherwise, with its waypoints and how far their end lies from the
        // target point.
        NavigationPathResult pathResultOf(
            int tileSize,
            const Route& route,
            glm::vec2 bodySize,
            Cell goal,
            glm::vec2 target)
        {
            NavigationPath path = waypointsOf(tileSize, route, bodySize);
            const float remaining = glm::distance(endOf(path), target);
            const NavigationPathStatus status = endOf(route).cell == goal
                                                    ? NavigationPathStatus::Found
                                                    : NavigationPathStatus::Unreachable;
            return {status, std::move(path), remaining};
        }

        // Grid steps between two cells: the fewest cost-one flights between them.
        int manhattanHeuristic(Cell cell, Cell goal)
        {
            return std::abs(cell.x - goal.x) + std::abs(cell.y - goal.y);
        }

        // Cost-one flight connections to adjacent open cells.
        std::vector<RouteConnection> flyingConnections(const TileMap& map, Cell cell)
        {
            constexpr std::array<glm::ivec2, 4> Directions{
                glm::ivec2{-1, 0}, glm::ivec2{1, 0}, glm::ivec2{0, -1}, glm::ivec2{0, 1}};

            std::vector<RouteConnection> connections;
            for (const glm::ivec2 direction : Directions)
            {
                const Cell candidate{cell.x + direction.x, cell.y + direction.y};
                if (map.contains(candidate) && !map.blocksMovement(candidate))
                {
                    connections.push_back({{{candidate}, Traversal::Fly, {}}, 1});
                }
            }
            return connections;
        }

        // The cheapest flight from the cell at the flyer's feet to the goal cell: every
        // cell that allows movement is a node, joined to its four neighbors at a cost
        // of one. No result when the flyer's feet are off the map.
        std::optional<NavigationPathResult> findFlyingPath(
            const TileMap& map,
            const Aabb& body,
            glm::vec2 target,
            FrameProfile* profile)
        {
            if (!isFinite(target))
            {
                throw std::invalid_argument("A navigation target must be finite");
            }
            if (!isFinite(body.position) || !isFinite(body.size) || body.size.x <= 0.0F ||
                body.size.y <= 0.0F)
            {
                throw std::invalid_argument("A flying body must be finite and positive-sized");
            }
            addFrameStatistic(profile, "Navigation", "Path searches");
            const int tileSize = map.tileSize();
            const Cell start = cellAtFeet(tileSize, feetOf(body));
            if (!map.contains(start))
            {
                return std::nullopt;
            }
            const Cell goal = cellAtFeet(tileSize, target);
            int cellsExpanded = 0;
            const ConnectionFunction connections =
                [&map, profile, &cellsExpanded](RouteLocation location)
            {
                const PhaseScope connectionPhase(profile, "Navigation", "Connection retrieval");
                ++cellsExpanded;
                return flyingConnections(map, location.cell);
            };
            RouteSearchResult result;
            {
                const PhaseScope algorithmPhase(profile, "Navigation", "Search algorithm");
                result =
                    findLowestCostRoute({start}, goal, map.size(), connections, manhattanHeuristic);
            }
            addFrameStatistic(profile, "Navigation", "Cells expanded", cellsExpanded);
            if (!result.route.has_value())
            {
                throw std::logic_error("A completed path search returned no path");
            }
            return pathResultOf(tileSize, *result.route, body.size, goal, target);
        }

        // Optimistic remaining travel time in ticks from a cell to the goal cell, at the
        // profile's fastest speed. The feet of a body in the one cell and in the other
        // lie at least the whole columns between them apart, whatever surface it holds.
        // Acceleration, braking, obstacles, and vertical travel are ignored.
        int platformerTickHeuristic(
            int tileSize,
            Cell cell,
            Cell goal,
            const PlatformerTraversalProfile& profile)
        {
            requirePositiveSeconds(profile.stepSeconds, "Navigation simulation step");
            if (!std::isfinite(profile.movement.maximumSpeed) ||
                profile.movement.maximumSpeed < 0.0F)
            {
                throw std::invalid_argument(
                    "Platformer navigation maximum speed must be finite and non-negative");
            }
            float maximumSpeed = profile.movement.maximumSpeed;
            if (profile.climb.has_value())
            {
                validateSurfaceClimbConfig(*profile.climb);
                maximumSpeed = std::max(maximumSpeed, profile.climb->speed);
            }

            const int columnsBetween = std::abs(goal.x - cell.x) - 1;
            if (columnsBetween <= 0 || maximumSpeed == 0.0F)
            {
                return 0;
            }
            const float distance = static_cast<float>(columnsBetween * tileSize);
            return static_cast<int>(std::ceil(distance / (maximumSpeed * profile.stepSeconds)));
        }

        // Adjust this search's copy, not the simulated travel costs shared by the cache.
        void applyJumpStartPenalty(std::vector<RouteConnection>& connections, int penaltyTicks)
        {
            for (RouteConnection& connection : connections)
            {
                if (connection.step.traversal != Traversal::Jump)
                {
                    continue;
                }
                if (connection.cost > std::numeric_limits<int>::max() - penaltyTicks)
                {
                    throw std::overflow_error("A route connection cost is too large");
                }
                connection.cost += penaltyTicks;
            }
        }

        void requireValid(glm::vec2 target, const PlatformerTraversalProfile& profile)
        {
            requirePositiveSeconds(profile.stepSeconds, "Navigation simulation step");
            if (!isFinite(target))
            {
                throw std::invalid_argument("A navigation target must be finite");
            }
            if (profile.climb.has_value())
            {
                validateSurfaceClimbConfig(*profile.climb);
            }
        }

        // The cheapest route over the floors, and for a climber the walls and ceilings,
        // from where the body rests to the goal cell. The search only reads the cache
        // and never simulates: a cell the cache does not hold yet is queued for the
        // fill, moved to the front, and defers the search. No result when the body rests
        // nowhere.
        std::optional<NavigationPathResult> findPlatformerPath(
            const TileMap& map,
            const Aabb& body,
            glm::vec2 target,
            const PlatformerTraversalProfile& profile,
            PlatformerConnectionCache& cache,
            FrameProfile* frameProfile)
        {
            requireValid(target, profile);
            addFrameStatistic(frameProfile, "Navigation", "Path searches");
            const std::optional<RouteLocation> resting = restingLocationOf(map, body, profile);
            if (!resting.has_value())
            {
                return std::nullopt;
            }
            const RouteLocation start = *resting;
            const int tileSize = map.tileSize();
            const Cell goal = cellAtFeet(tileSize, target);
            {
                const PhaseScope cachePhase(frameProfile, "Navigation", "Path cache");
                cache.applyRecordedTileBreaks(map);
            }

            int cellsExpanded = 0;
            const ConnectionFunction connections =
                [&profile, &cache, frameProfile, &cellsExpanded](RouteLocation location)
            {
                const PhaseScope connectionPhase(
                    frameProfile, "Navigation", "Connection retrieval");
                ++cellsExpanded;

                // 1. Read the cell's connections from the cache. One entry holds the
                //    connections leaving every surface of the cell. The search only
                //    expands cells the cache holds, so this never misses.
                const std::vector<RouteConnection>* cellConnections =
                    cache.cachedConnections(location.cell, profile);
                if (cellConnections == nullptr)
                {
                    throw std::logic_error("The search expanded a cell the cache does not hold");
                }

                // 2. Keep the connections that leave this location's surface. A floor
                //    expands with floor connections, a wall with that wall's climbs.
                std::vector<RouteConnection> leaving;
                for (const RouteConnection& connection : *cellConnections)
                {
                    if (connection.sourceSurface == location.surface)
                    {
                        leaving.push_back(connection);
                    }
                }

                // 3. Charge each jump the start penalty. This changes the search's copy
                //    only; the cached costs stay the simulated ticks.
                applyJumpStartPenalty(leaving, JumpStartPenaltyTicks);
                return leaving;
            };
            const HeuristicFunction heuristic = [tileSize, &profile](Cell cell, Cell goalCell)
            { return platformerTickHeuristic(tileSize, cell, goalCell, profile); };
            // A cell the cache does not hold yet pauses the search; the fill builds it.
            const ExpansionReady canExpand = [&cache, &profile](RouteLocation location)
            { return cache.cachedConnections(location.cell, profile) != nullptr; };
            RouteSearchResult result;
            {
                const PhaseScope algorithmPhase(frameProfile, "Navigation", "Search algorithm");
                result =
                    findLowestCostRoute(start, goal, map.size(), connections, heuristic, canExpand);
            }
            addFrameStatistic(frameProfile, "Navigation", "Cells expanded", cellsExpanded);

            if (result.unexpandedLocation.has_value())
            {
                {
                    const PhaseScope cachePhase(frameProfile, "Navigation", "Path cache");
                    cache.queue(result.unexpandedLocation->cell, profile);
                    cache.prioritise(result.unexpandedLocation->cell, profile);
                }
                addFrameStatistic(frameProfile, "Navigation", "Paths deferred");
                return NavigationPathResult{NavigationPathStatus::Deferred, std::nullopt};
            }
            if (!result.route.has_value())
            {
                throw std::logic_error("A completed path search returned no path");
            }
            return pathResultOf(tileSize, *result.route, profile.size, goal, target);
        }
    }

    PlatformerTraversalProfile platformerTraversalProfileFor(const Actor& actor, float stepSeconds)
    {
        if (!actor.platformerMovement.has_value())
        {
            throw std::invalid_argument(
                "A platformer traversal profile requires platformer movement");
        }
        return {
            actor.body.bounds.size,
            actor.platformerMovement->config,
            stepSeconds,
            actor.surfaceClimb.has_value()
                ? std::optional<SurfaceClimbConfig>{actor.surfaceClimb->config}
                : std::nullopt};
    }

    std::optional<NavigationPathResult> findActorPath(
        const TileMap& map,
        const Actor& actor,
        glm::vec2 target,
        float stepSeconds,
        PlatformerConnectionCache& cache,
        FrameProfile* frameProfile)
    {
        if (actor.flyingMovement.has_value())
        {
            return findFlyingPath(map, actor.body.bounds, target, frameProfile);
        }
        if (!actor.platformerMovement.has_value())
        {
            return std::nullopt;
        }
        return findPlatformerPath(
            map,
            actor.body.bounds,
            target,
            platformerTraversalProfileFor(actor, stepSeconds),
            cache,
            frameProfile);
    }
}
