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
    // steps a walk, a fall or a jump. No path when either cell is off the map. With
    // statistics, reports what the search cost. With a cache for this map, reads each
    // cell's connections from it, simulating and keeping them first when it lacks them;
    // answers a query it has answered before with the path it kept; and when a search
    // from the start has failed before, answers without searching unless the goal is
    // among the cells that start reaches. A cell still waiting for the fill is not
    // simulated: the search moves it to the front of the queue and, if it found no path
    // without it, reports itself deferred in the statistics and keeps nothing, so the
    // caller asks again once the fill has caught up.
    std::optional<NavigationPath> findPlatformerPath(
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
