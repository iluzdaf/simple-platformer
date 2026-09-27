#pragma once

#include <optional>

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_search.hpp"

namespace simple_platformer
{
    class PlatformerConnectionCache;
    class TileMap;

    struct PlatformerNavigationConfig
    {
        // Added whenever a route starts a jump, so a small time saving does not
        // make grounded actors hop unnecessarily.
        int jumpStartPenaltyTicks = 30;
    };

    enum class PlatformerPathStatus
    {
        Found,
        Unreachable,
        Deferred
    };

    // Only Found carries a path; Deferred means the background fill may change the answer.
    struct PlatformerPathResult
    {
        PlatformerPathStatus status = PlatformerPathStatus::Unreachable;
        std::optional<NavigationPath> path;
    };

    // Every search below takes the fixed step the actor is moved with, in seconds.
    // Connections are simulated tick by tick at that step with the real movement code
    // and costs are counted in its ticks, so a predicted jump and the real one run the
    // same physics. It must be finite and positive.

    // Optimistic remaining travel time in simulation ticks.
    int platformerTickHeuristic(
        int tileSize,
        GridPosition cell,
        GridPosition goal,
        const PlatformerMovementConfig& movement,
        float stepSeconds);

    // The cheapest route for a platformer body from one cell to another, each of its
    // steps a walk, a fall or a jump. An off-map cell is Unreachable. With
    // statistics, reports what the search cost. With a cache for this map, reads each
    // cell's connections from it, simulating and keeping them first when it lacks them;
    // answers a query it has answered before with the path it kept; and when a search
    // from the start has failed before, answers without searching unless the goal is
    // among the cells that start reaches. A search waiting for the fill is Deferred,
    // so the caller should retry without its normal repath cooldown. Only Found has
    // a path.
    PlatformerPathResult findPlatformerPath(
        const TileMap& map,
        GridPosition start,
        GridPosition goal,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement,
        float stepSeconds,
        const PlatformerNavigationConfig& navigation = {},
        PathSearchStatistics* statistics = nullptr,
        PlatformerConnectionCache* cache = nullptr);
}
