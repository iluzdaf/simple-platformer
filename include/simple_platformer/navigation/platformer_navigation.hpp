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

    // Whether the body can stand in the cell: the cell blocks nothing, nor does any cell
    // the body covers standing there, and the cell below blocks movement.
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

    // The connections leaving a cell, as a copy the caller owns: walks to every cell
    // along the floor either way, and the cheapest fall and jumps to either side that
    // land on a standable cell. With a cache, taken from it or kept in it as
    // platformerNeighborsKept does; without one, simulated for this call alone. With
    // statistics, adds the movement ticks simulated, or counts the cell as reused.
    std::vector<NavigationNeighbor> platformerNeighbors(
        const TileMap& map,
        GridPosition cell,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement,
        float stepSeconds,
        PathSearchStatistics* statistics = nullptr,
        PlatformerConnectionCache* cache = nullptr);

    // What keeping a cell costs a fill's budget besides the ticks it simulated: the
    // bookkeeping, worth about this many ticks, so a run of cells that cannot be stood
    // on is spread over steps like the rest.
    constexpr int KeepCostTicks = 3;

    // What one fill call did: the cells it kept, the movement ticks that took, and the
    // budget it spent, which is the ticks plus the keep cost of each cell.
    struct FillWork
    {
        int cells = 0;
        int simulatedTicks = 0;
        int budgetSpent = 0;
    };

    // Simulates and keeps the cells waiting in the cache's queue for this body, in its
    // order, until the budget is spent or none are left. A cell is never split, so a
    // call may run one cell past the budget.
    FillWork fillPlatformerConnections(
        const TileMap& map,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement,
        float stepSeconds,
        PlatformerConnectionCache& cache,
        int tickBudget);

    // Queues every cell of the map the cache lacks for this body, so fills keep them over
    // the calls that follow. Cells kept or waiting already are left as they are.
    void queueAllPlatformerConnections(
        const TileMap& map,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement,
        float stepSeconds,
        PlatformerConnectionCache& cache);

    // Keeps the connections leaving every cell of the map for this body at once,
    // simulating any the cache lacks. Cells already kept are left as they are, so calling
    // it again fills only what has since been emptied. Tests use it to start from a full
    // cache; the game queues the cells and fills them over its steps instead.
    void keepAllPlatformerConnections(
        const TileMap& map,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement,
        float stepSeconds,
        PlatformerConnectionCache& cache);

    // The connections leaving a cell, read from the cache rather than copied out of it:
    // simulated and kept first when the cache lacks them. The reference holds until a
    // break drops the cell or the cache is cleared. With statistics, counts the cell as
    // reused when it was kept already.
    const std::vector<NavigationNeighbor>& platformerNeighborsKept(
        const TileMap& map,
        GridPosition cell,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement,
        float stepSeconds,
        PlatformerConnectionCache& cache,
        PathSearchStatistics* statistics = nullptr);
}
