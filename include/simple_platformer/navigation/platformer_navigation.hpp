#pragma once

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

namespace simple_platformer
{
    class PlatformerConnectionCache;
    class TileMap;
    struct FrameProfile;

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

    // Finds the cheapest walk, fall, and jump route using the cache for this map.
    // Missing connections are simulated unless queued for fill. A cached path or
    // proven failure can answer without searching. Off-map endpoints are Unreachable;
    // pending cells return Deferred so the caller can retry next step. Only Found
    // carries a path. An optional frame profile records search and simulation work.
    NavigationPathResult findPlatformerPath(
        const TileMap& map,
        GridPosition start,
        GridPosition goal,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement,
        float stepSeconds,
        PlatformerConnectionCache& cache,
        const PlatformerNavigationConfig& navigation = {},
        FrameProfile* profile = nullptr);
}
