#pragma once

#include <vector>

#include "simple_platformer/navigation/connection_cache.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"

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
