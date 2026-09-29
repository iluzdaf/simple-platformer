#pragma once

#include "simple_platformer/navigation/platformer_connection_cache.hpp"
#include "simple_platformer/navigation/navigation_fill.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/fixed_step.hpp"

namespace tests
{
    // Queues and caches every cell for each platformer NPC profile, as the game does
    // over the first steps of a level.
    inline void prepareNavigationCache(
        const simple_platformer::TileMap& map,
        simple_platformer::World& world)
    {
        simple_platformer::queueNavigationFill(map, world, FixedStepSeconds);
        simple_platformer::PlatformerConnectionCache& cache = world.platformerConnections();
        while (simple_platformer::advanceNavigationFill(
                   map, cache, simple_platformer::NavigationFillTicksPerStep) > 0)
        {
        }
    }
}
