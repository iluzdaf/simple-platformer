#pragma once

#include <vector>

#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/connection_cache.hpp"

namespace simple_platformer
{
    class TileMap;
    class World;

    // A connection cache is filled through its queue, never all at once during play:
    // every cell of the map is queued for each body when a level starts, the cells a
    // break drops join the queue after, and a fill each simulation step keeps a few of
    // them. Searches that need a cell the fill has not reached wait for it.

    // The movement ticks one simulation step may spend keeping queued cells, shared out
    // evenly among the bodies with cells waiting: a few cells a step, so a level start
    // or a break costs a little on each of the steps that follow instead of everything
    // on one, however many bodies the level has. A cell is never split, so a body may
    // run one cell past its share.
    constexpr int NavigationFillTicksPerStep = 250;

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

    // The distinct platformer NPC bodies in the world, at the step the NPCs will be
    // simulated with, in the order first met: what a level queues navigation for.
    std::vector<ConnectionBody> platformerBodiesIn(const World& world, float stepSeconds);

    // Queues every cell of the map for each body. Call once when the level starts.
    void queueNavigation(
        const TileMap& map,
        const std::vector<ConnectionBody>& bodies,
        PlatformerConnectionCache& cache);

    // Simulates and keeps some of the cells queued for each body the cache knows,
    // within the budget, and reports what it did over every body. Runs every step
    // before the NPCs think.
    FillWork fillNavigation(const TileMap& map, PlatformerConnectionCache& cache, int tickBudget);
}
