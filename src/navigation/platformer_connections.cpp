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
#include "simple_platformer/navigation/platformer_cells.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    namespace
    {
        // A traversal must land and stop within this many updates to become a
        // connection. Its duration in seconds depends on the caller's step.
        constexpr int MaximumConnectionSimulationTicks = 120;

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

        // Include one tile around the bounds because collision and support checks read
        // tiles beside the body as well as under it.
        void includeCellsAroundBounds(CellRange& accumulatedCells, int tileSize, const Aabb& bounds)
        {
            const glm::vec2 margin{static_cast<float>(tileSize), static_cast<float>(tileSize)};
            const Aabb around{bounds.position - margin, bounds.size + 2.0F * margin};
            accumulatedCells = unionOf(accumulatedCells, cellsCovered(tileSize, around));
        }

        // Simulates a complete start-to-stop walk using the real path follower, movement,
        // and collision code. Returns its fixed-update cost, or no cost when the actor
        // cannot reach and stop at the destination within the connection simulation
        // limit, with the cells it swept as offsets from the start.
        WalkSimulationResult simulateWalk(
            const TileMap& map,
            GridPosition start,
            GridPosition destinationCell,
            glm::vec2 bodySize,
            const PlatformerMovementConfig& config,
            float stepSeconds)
        {
            const int tileSize = map.tileSize();
            Body body{boxInCell(tileSize, start, bodySize), {0.0F, 0.0F}};
            PlatformerMovement movement{config, true, 0.0F, 0.0F};
            PathFollower follower;
            setPath(follower, {start, {{destinationCell, Traversal::Walk, {}}}}, destinationCell);

            WalkSimulationResult walk{std::nullopt, cellsCovered(tileSize, body.bounds), 0};
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
                includeCellsAroundBounds(walk.sweep, tileSize, body.bounds);
                ++walk.simulatedTicks;
            }
            walk.sweep = {
                {walk.sweep.first.x - start.x, walk.sweep.first.y - start.y},
                {walk.sweep.last.x - start.x, walk.sweep.last.y - start.y}};
            return walk;
        }

        bool touchesHorizontalMapEdge(const TileMap& map, const Aabb& bounds, float direction)
        {
            return (direction < 0.0F && bounds.position.x <= EdgeTolerance) ||
                   (direction > 0.0F &&
                    bounds.position.x + bounds.size.x >= map.pixelWidth() - EdgeTolerance);
        }

        enum class PlatformerManeuver
        {
            Walk,
            Fall,
            Jump
        };

        struct ManeuverAttempt
        {
            PlatformerManeuver maneuver;
            int direction;
            int jumpHoldTicks = 0;
        };

        // The inputs of a fall or a jump at this tick: pushing one way until landed, and
        // for a jump, pressing on the first tick and holding for as many as asked.
        InputIntentions makeTraversalIntentions(
            PlatformerManeuver maneuver,
            float direction,
            int tick,
            int jumpHoldTicks,
            bool hasLanded)
        {
            InputIntentions intentions;
            intentions.direction.x = hasLanded ? 0.0F : direction;
            if (maneuver == PlatformerManeuver::Jump && !hasLanded)
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

        struct AirborneSimulationResult
        {
            std::optional<GridPosition> landingCell;
            int simulatedTicks = 0;
            InputProgram inputs;
            CellRange footprint;
        };

        // Simulates leaving the ground, landing on another standable cell, and braking
        // to a stop. An unsuccessful attempt has no landing, but still reports its
        // simulated ticks and footprint.
        AirborneSimulationResult simulateAirborneTraversal(
            const TileMap& map,
            GridPosition start,
            const PlatformerTraversalProfile& profile,
            const ManeuverAttempt& attempt)
        {
            Body body{boxInCell(map.tileSize(), start, profile.size), {0.0F, 0.0F}};
            PlatformerMovement movement{profile.movement, true, 0.0F, 0.0F};
            AirborneSimulationResult result{
                std::nullopt, 0, {}, cellsCovered(map.tileSize(), body.bounds)};
            bool leftGround = false;
            std::optional<GridPosition> landing;
            const float direction = static_cast<float>(attempt.direction);

            for (int tick = 0; tick < MaximumConnectionSimulationTicks; ++tick)
            {
                if (touchesHorizontalMapEdge(map, body.bounds, direction))
                {
                    return result;
                }

                const InputIntentions intentions = makeTraversalIntentions(
                    attempt.maneuver, direction, tick, attempt.jumpHoldTicks, landing.has_value());
                recordSimulationInput(result.inputs, intentions, profile.stepSeconds);
                updatePlatformerMovement(map, body, movement, intentions, profile.stepSeconds);
                includeCellsAroundBounds(result.footprint, map.tileSize(), body.bounds);
                ++result.simulatedTicks;

                leftGround = leftGround || !movement.grounded;
                if (!leftGround || !movement.grounded)
                {
                    continue;
                }

                if (!landing.has_value())
                {
                    landing = tryFindLandingCell(map, start, body.bounds, profile.size);
                    if (!landing.has_value())
                    {
                        return result;
                    }
                }
                if (body.velocity.x != 0.0F)
                {
                    continue;
                }
                const GridPosition stoppedCell = cellAtFeet(map.tileSize(), feetOf(body.bounds));
                if (stoppedCell != landing.value())
                {
                    return result;
                }
                result.landingCell = stoppedCell;
                return result;
            }
            return result;
        }

        void keepCheapest(
            std::vector<NavigationConnection>& connections,
            NavigationConnection candidate)
        {
            const auto existing = std::find_if(
                connections.begin(),
                connections.end(),
                [&candidate](const NavigationConnection& connection)
                {
                    return connection.step.destinationCell == candidate.step.destinationCell &&
                           connection.step.traversal == candidate.step.traversal;
                });
            if (existing == connections.end())
            {
                connections.push_back(std::move(candidate));
            }
            else if (candidate.cost < existing->cost)
            {
                *existing = std::move(candidate);
            }
        }

        struct ConnectionPlan
        {
            std::vector<ManeuverAttempt> attempts;
            CellRange footprint;
        };

        // The policy decides which maneuvers to try; simulation later determines
        // whether they succeed and where they end.
        ConnectionPlan planPlatformerConnections(
            const TileMap& map,
            GridPosition start,
            glm::vec2 bodySize)
        {
            const int tileSize = map.tileSize();
            ConnectionPlan plan{{}, cellsCovered(tileSize, boxInCell(tileSize, start, bodySize))};
            const auto recordProbe = [&](GridPosition cell) {
                includeCellsAroundBounds(
                    plan.footprint, tileSize, boxInCell(tileSize, cell, bodySize));
            };
            recordProbe(start);
            if (!canStandAt(map, start, bodySize))
            {
                return plan;
            }

            constexpr std::array<int, 2> Directions{-1, 1};
            constexpr std::array<int, 2> JumpHoldTicks{1, MaximumConnectionSimulationTicks};
            plan.attempts.reserve(Directions.size() * (1 + JumpHoldTicks.size()));
            for (const int direction : Directions)
            {
                const GridPosition adjacent{start.x + direction, start.y};
                recordProbe(adjacent);
                const PlatformerManeuver ground = canStandAt(map, adjacent, bodySize)
                                                      ? PlatformerManeuver::Walk
                                                      : PlatformerManeuver::Fall;
                plan.attempts.push_back({ground, direction});
                for (const int holdTicks : JumpHoldTicks)
                {
                    plan.attempts.push_back({PlatformerManeuver::Jump, direction, holdTicks});
                }
            }
            return plan;
        }

        // The adjacent cell has already passed the standability check.
        BuiltPlatformerConnections buildWalkConnections(
            const TileMap& map,
            GridPosition start,
            const PlatformerTraversalProfile& profile,
            const PlatformerConnectionCache* walkCache,
            int direction)
        {
            const int tileSize = map.tileSize();
            BuiltPlatformerConnections result{
                {}, cellsCovered(tileSize, boxInCell(tileSize, start, profile.size)), {}, 0};
            GridPosition destination{start.x + direction, start.y};
            do
            {
                const int columns = destination.x - start.x;
                const WalkSimulationResult* cached =
                    walkCache != nullptr ? walkCache->cachedWalk(columns, profile) : nullptr;
                const WalkSimulationResult walk = cached != nullptr ? *cached
                                                                    : simulateWalk(
                                                                          map,
                                                                          start,
                                                                          destination,
                                                                          profile.size,
                                                                          profile.movement,
                                                                          profile.stepSeconds);
                if (cached == nullptr)
                {
                    result.simulatedTicks += walk.simulatedTicks;
                    if (walkCache != nullptr)
                    {
                        result.walksToCache.push_back({columns, walk});
                    }
                }
                result.footprint = unionOf(
                    result.footprint,
                    {{start.x + walk.sweep.first.x, start.y + walk.sweep.first.y},
                     {start.x + walk.sweep.last.x, start.y + walk.sweep.last.y}});
                if (!walk.cost.has_value())
                {
                    // A failed walk ends this direction's search; farther cells are not tried.
                    break;
                }
                result.connections.push_back(
                    {{destination, Traversal::Walk, {}}, walk.cost.value()});
                destination.x += direction;
                includeCellsAroundBounds(
                    result.footprint, tileSize, boxInCell(tileSize, destination, profile.size));
            } while (canStandAt(map, destination, profile.size));
            return result;
        }

        BuiltPlatformerConnections buildAirborneConnection(
            const TileMap& map,
            GridPosition start,
            const PlatformerTraversalProfile& profile,
            const ManeuverAttempt& attempt)
        {
            AirborneSimulationResult simulated =
                simulateAirborneTraversal(map, start, profile, attempt);
            BuiltPlatformerConnections result{
                {}, simulated.footprint, {}, simulated.simulatedTicks};
            if (simulated.landingCell.has_value())
            {
                const Traversal traversal = attempt.maneuver == PlatformerManeuver::Fall
                                                ? Traversal::Fall
                                                : Traversal::Jump;
                result.connections.push_back(
                    {{simulated.landingCell.value(), traversal, std::move(simulated.inputs)},
                     simulated.simulatedTicks});
            }
            return result;
        }

    }

    BuiltPlatformerConnections buildPlatformerConnections(
        const TileMap& map,
        GridPosition cell,
        const PlatformerTraversalProfile& profile,
        const PlatformerConnectionCache* walkCache)
    {
        requirePositiveSeconds(profile.stepSeconds, "Navigation simulation step");
        const ConnectionPlan plan = planPlatformerConnections(map, cell, profile.size);
        BuiltPlatformerConnections combined{{}, plan.footprint, {}, 0};
        for (const ManeuverAttempt& attempt : plan.attempts)
        {
            BuiltPlatformerConnections attemptResult;
            switch (attempt.maneuver)
            {
            case PlatformerManeuver::Walk:
                attemptResult =
                    buildWalkConnections(map, cell, profile, walkCache, attempt.direction);
                break;
            case PlatformerManeuver::Fall:
            case PlatformerManeuver::Jump:
                attemptResult = buildAirborneConnection(map, cell, profile, attempt);
                break;
            }
            combined.footprint = unionOf(combined.footprint, attemptResult.footprint);
            combined.simulatedTicks += attemptResult.simulatedTicks;
            for (NavigationConnection& connection : attemptResult.connections)
            {
                keepCheapest(combined.connections, std::move(connection));
            }
            for (const SimulatedWalk& walk : attemptResult.walksToCache)
            {
                combined.walksToCache.push_back(walk);
            }
        }
        return combined;
    }

    void storePlatformerConnections(
        PlatformerConnectionCache& cache,
        GridPosition cell,
        const PlatformerTraversalProfile& profile,
        BuiltPlatformerConnections built)
    {
        for (const SimulatedWalk& walk : built.walksToCache)
        {
            cache.storeWalk(walk.columns, profile, walk.result);
        }
        cache.storeConnections(cell, profile, std::move(built.connections), built.footprint);
    }

}
