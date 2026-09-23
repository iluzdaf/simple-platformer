#pragma once

#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_search.hpp"

namespace simple_platformer
{
    struct Aabb;
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

    // High-level path API for platformer actors. With statistics, reports what the search
    // cost: the cells it expanded and the movement ticks it simulated. With a cache for
    // this map, takes each cell's connections from it when they are there and keeps them
    // there when they are not, so no cell is simulated twice.
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

    bool canStandAt(const TileMap& map, GridPosition cell, glm::vec2 bodySize);

    // Finds the closest standable cell beneath a grounded body. The body's feet may
    // extend beyond a ledge while part of its collider is still supported.
    std::optional<GridPosition> findPlatformerStartCell(const TileMap& map, const Aabb& bounds);

    // Keeps a standable target cell; otherwise chooses the closest standable feet
    // position for this NPC's body size. Ties use row, then column order.
    // This selects a chase destination, not a guaranteed path to it.
    std::optional<GridPosition> findPlatformerChaseCell(
        const TileMap& map,
        glm::vec2 lastSeenFeet,
        glm::vec2 bodySize);

    // Lower-level policy used by the generic path search. With statistics, adds the
    // movement ticks it simulated, or counts the cell as reused when a cache held it.
    std::vector<NavigationNeighbor> platformerNeighbors(
        const TileMap& map,
        GridPosition cell,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement,
        float stepSeconds,
        PathSearchStatistics* statistics = nullptr,
        PlatformerConnectionCache* cache = nullptr);

    // Simulates and keeps the connections leaving every cell of the map for this body, so
    // no search has to during play. Cells already kept are left as they are.
    void keepAllPlatformerConnections(
        const TileMap& map,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement,
        float stepSeconds,
        PlatformerConnectionCache& cache);

    // The same connections as the cache keeps them, simulated and kept first when it does
    // not yet, and read where they are rather than copied out. The reference holds until
    // the cache is cleared.
    const std::vector<NavigationNeighbor>& platformerNeighborsKept(
        const TileMap& map,
        GridPosition cell,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement,
        float stepSeconds,
        PlatformerConnectionCache& cache,
        PathSearchStatistics* statistics = nullptr);
}
