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
        // Added to the cost of every jump, so saving a little time does not make a
        // grounded actor hop.
        constexpr int JumpStartPenaltyTicks = 30;

        // Every place in a cell a body can rest: its floor, walls and ceiling.
        constexpr std::array<ClimbSurface, 4> Surfaces{
            ClimbSurface::None,
            ClimbSurface::LeftWall,
            ClimbSurface::RightWall,
            ClimbSurface::Ceiling};

        // In pixels: how far a body may sit off a surface and still rest on it.
        constexpr float RestingTolerance = 1.0F;

        // Where a body is resting, judged from its bounds alone. Of the places near it the
        // profile can rest at, it picks the one whose resting bounds are within a pixel of
        // the body across the surface, and nearest along it, up to a tile away. Nothing if
        // the body rests nowhere, such as in the air.
        std::optional<RouteLocation> restingLocationOf(
            const TileMap& map,
            const Aabb& bounds,
            const PlatformerTraversalProfile& profile)
        {
            if (!isFinite(bounds.topLeft))
            {
                throw std::invalid_argument("A resting body must have a finite position");
            }
            if (!isFinitePositive(bounds.size))
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
                            bounds.topLeft -
                            boundsAtSurface(tileSize, candidate, bounds.size).topLeft;
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

        // Turns a route into a path: the body's resting feet at each place on the route.
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

        // What the caller gets from a route: Found if it ends in the goal cell, Unreachable
        // if not, with the path and how far the path's end is from the goal.
        NavigationPathResult pathResultOf(
            int tileSize,
            const Route& route,
            glm::vec2 bodySize,
            Cell goal,
            glm::vec2 goalFeet)
        {
            NavigationPath path = waypointsOf(tileSize, route, bodySize);
            const float remaining = glm::distance(endOf(path), goalFeet);
            const NavigationPathStatus status = endOf(route).cell == goal
                                                    ? NavigationPathStatus::Found
                                                    : NavigationPathStatus::Unreachable;
            return {status, std::move(path), remaining};
        }

        // The number of cells between two cells along the grid. Each flight moves one cell
        // for a cost of 1, so the guess is never more than the real cost.
        int manhattanHeuristic(Cell cell, Cell goal)
        {
            return std::abs(cell.x - goal.x) + std::abs(cell.y - goal.y);
        }

        // A flight to each open cell next to this one, for a cost of 1.
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

        // The cheapest flight from the cell at the flyer's feet to the goal cell, through
        // open cells. No result if the flyer's feet are off the map.
        std::optional<NavigationPathResult> findFlyingPath(
            const TileMap& map,
            const Aabb& body,
            glm::vec2 goalFeet,
            FrameProfile* profile)
        {
            requireFinite(goalFeet, "A navigation goal");
            if (!isFinite(body.topLeft) || !isFinitePositive(body.size))
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
            const Cell goal = cellAtFeet(tileSize, goalFeet);
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
                throw std::logic_error("A completed route search returned no route");
            }
            return pathResultOf(tileSize, *result.route, body.size, goal, goalFeet);
        }

        // Guesses the ticks left from a cell to the goal cell: the time to cross the whole
        // columns between them at the profile's fastest speed. A body's feet in the one
        // cell and in the other are at least that far apart, whatever surface it holds,
        // and the guess leaves out acceleration, braking, obstacles and height, so it is
        // never more than the real cost.
        int platformerTickHeuristic(
            int tileSize,
            Cell cell,
            Cell goal,
            const PlatformerTraversalProfile& profile)
        {
            requirePositiveSeconds(profile.stepSeconds, "Navigation simulation step");
            if (!isFiniteNonNegative(profile.movement.maximumSpeed))
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

        // Adds the penalty to each jump in this search's copy of the connections. The
        // cached costs stay the simulated ticks.
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

        void requireValid(glm::vec2 goalFeet, const PlatformerTraversalProfile& profile)
        {
            requirePositiveSeconds(profile.stepSeconds, "Navigation simulation step");
            requireFinite(goalFeet, "A navigation goal");
            if (profile.climb.has_value())
            {
                validateSurfaceClimbConfig(*profile.climb);
            }
        }

        // The cheapest path over floors, and for a climber walls and ceilings, from where
        // the body rests to the goal cell. It only reads the cache and never runs movement.
        // If the cache does not hold a cell the search needs yet, that cell goes to the
        // front of the fill and the result is Deferred. No result if the body rests
        // nowhere.
        std::optional<NavigationPathResult> findPlatformerPath(
            const TileMap& map,
            const Aabb& body,
            glm::vec2 goalFeet,
            const PlatformerTraversalProfile& profile,
            PlatformerConnectionCache& cache,
            FrameProfile* frameProfile)
        {
            requireValid(goalFeet, profile);

            addFrameStatistic(frameProfile, "Navigation", "Path searches");

            const std::optional<RouteLocation> resting = restingLocationOf(map, body, profile);
            if (!resting.has_value())
            {
                return std::nullopt;
            }

            const RouteLocation start = *resting;
            const int tileSize = map.tileSize();
            const Cell goal = cellAtFeet(tileSize, goalFeet);
            {
                const PhaseScope cachePhase(frameProfile, "Navigation", "Path cache");
                cache.applyRecordedTileBreaks(map, frameProfile);
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

            // The search pauses at a cell the cache does not hold yet, until the fill
            // builds it.
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
                throw std::logic_error("A completed route search returned no route");
            }
            return pathResultOf(tileSize, *result.route, profile.size, goal, goalFeet);
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
        glm::vec2 goalFeet,
        float stepSeconds,
        PlatformerConnectionCache& cache,
        FrameProfile* frameProfile)
    {
        if (actor.flyingMovement.has_value())
        {
            return findFlyingPath(map, actor.body.bounds, goalFeet, frameProfile);
        }
        if (!actor.platformerMovement.has_value())
        {
            return std::nullopt;
        }
        return findPlatformerPath(
            map,
            actor.body.bounds,
            goalFeet,
            platformerTraversalProfileFor(actor, stepSeconds),
            cache,
            frameProfile);
    }
}
