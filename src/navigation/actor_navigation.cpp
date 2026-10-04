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
#include "simple_platformer/navigation/route.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/route_search.hpp"
#include "simple_platformer/navigation/platformer_cells.hpp"
#include "simple_platformer/navigation/platformer_connection_table.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/navigation/traversal.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

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

        // Finds where the body rests, using its bounds rather than its movement state.
        // Candidates must fit the body and be within RestingTolerance across the surface.
        // Chooses the nearest along the surface, less than a tile away; nothing if none fit.
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
            // Include neighbouring cells: a supporting surface may sit just outside
            // the cells overlapped by the body.
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
                        // Across measures separation from the surface; along measures
                        // distance to the candidate's resting position on that surface.
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

        // Turns a successful route into a Found result, with waypoints and the distance
        // from the final feet to the requested goal. The search has reached the goal cell.
        NavigationPathResult pathResultOf(
            int tileSize,
            const Route& route,
            glm::vec2 bodySize,
            glm::vec2 goalFeet)
        {
            NavigationPath path = waypointsOf(tileSize, route, bodySize);
            // Reaching the goal cell need not put the feet at the exact requested point.
            const float remaining = glm::distance(endOf(path), goalFeet);
            return {NavigationPathStatus::Found, std::move(path), remaining};
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
            glm::vec2 goalFeet)
        {
            requireFinite(goalFeet, "A navigation goal");
            if (!isFinite(body.topLeft) || !isFinitePositive(body.size))
            {
                throw std::invalid_argument("A flying body must be finite and positive-sized");
            }

            const int tileSize = map.tileSize();
            const Cell start = cellAtFeet(tileSize, feetOf(body));
            if (!map.contains(start))
            {
                return std::nullopt;
            }
            const Cell goal = cellAtFeet(tileSize, goalFeet);

            const ConnectionFunction connections = [&map](RouteLocation location)
            { return flyingConnections(map, location.cell); };
            const RouteSearchResult result =
                findLowestCostRoute({start}, goal, map.size(), connections, manhattanHeuristic);

            if (!result.route.has_value())
            {
                // A valid start but no route: Unreachable, rather than no search result.
                return NavigationPathResult{};
            }
            return pathResultOf(tileSize, *result.route, body.size, goalFeet);
        }

        // Guesses the ticks left from a cell to the goal cell: the time to cross the whole
        // columns between them at the profile's fastest speed. A body's feet in the one
        // cell and in the other are at least that far apart, whatever surface it holds,
        // and the guess leaves out acceleration, braking and obstacles, so it is never
        // more than the real cost. Height is ignored because jumps, falls and climbing
        // use different vertical speeds; dividing row distance by horizontal speed could
        // overestimate the remaining time. A* needs a guess that never overestimates.
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
                // Ceiling climbing can cover horizontal distance faster than walking.
                maximumSpeed = std::max(maximumSpeed, profile.climb->speed);
            }

            // Count only whole columns between the cells: feet may be near their
            // facing edges, so adjacent cells provide no minimum horizontal distance.
            const int columnsBetween = std::abs(goal.x - cell.x) - 1;
            if (columnsBetween <= 0 || maximumSpeed == 0.0F)
            {
                // Zero gives the search no guidance; it does not declare the goal unreachable.
                return 0;
            }
            const float distance = static_cast<float>(columnsBetween * tileSize);
            return static_cast<int>(std::ceil(distance / (maximumSpeed * profile.stepSeconds)));
        }

        // Adds the penalty to each jump in this search's copy of the connections. The
        // table's costs stay the simulated ticks.
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
        // the body rests to the goal cell. It prepares the profile in the table, then only
        // reads it. No result if the body rests nowhere.
        std::optional<NavigationPathResult> findPlatformerPath(
            const TileMap& map,
            const Aabb& body,
            glm::vec2 goalFeet,
            const PlatformerTraversalProfile& profile,
            PlatformerConnectionTable& table)
        {
            requireValid(goalFeet, profile);
            // Reuse simulated connections, updating them for tile breaks or a new profile
            // before the search reads the table.
            table.prepare(map, profile);

            const std::optional<RouteLocation> resting = restingLocationOf(map, body, profile);
            if (!resting.has_value())
            {
                return std::nullopt;
            }

            const RouteLocation start = *resting;
            const int tileSize = map.tileSize();
            const Cell goal = cellAtFeet(tileSize, goalFeet);

            const ConnectionFunction connections = [&table, &profile](RouteLocation location)
            {
                // Keep the connections that leave this location's surface. A floor
                // expands with floor connections, a wall with that wall's climbs. The
                // table holds the connections leaving every surface of the cell.
                std::vector<RouteConnection> leaving;
                for (const RouteConnection& connection : table.connections(location.cell, profile))
                {
                    if (connection.sourceSurface == location.surface)
                    {
                        leaving.push_back(connection);
                    }
                }

                applyJumpStartPenalty(leaving, JumpStartPenaltyTicks);

                return leaving;
            };

            const HeuristicFunction heuristic = [tileSize, &profile](Cell cell, Cell goalCell)
            { return platformerTickHeuristic(tileSize, cell, goalCell, profile); };

            const RouteSearchResult result =
                findLowestCostRoute(start, goal, map.size(), connections, heuristic);

            if (!result.route.has_value())
            {
                return NavigationPathResult{};
            }
            return pathResultOf(tileSize, *result.route, profile.size, goalFeet);
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
        PlatformerConnectionTable& connections)
    {
        if (actor.flyingMovement.has_value())
        {
            return findFlyingPath(map, actor.body.bounds, goalFeet);
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
            connections);
    }

    void prepareNavigation(const TileMap& map, World& world, float stepSeconds)
    {
        requirePositiveSeconds(stepSeconds, "Navigation step");
        PlatformerConnectionTable& table = world.platformerConnections();
        // Actors with the same profile share a table; preparing it again reuses the build.
        for (const Actor& actor : world.actors())
        {
            if (actor.pathFollower.has_value() && actor.platformerMovement.has_value())
            {
                table.prepare(map, platformerTraversalProfileFor(actor, stepSeconds));
            }
        }
    }
}
