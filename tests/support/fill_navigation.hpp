#pragma once

#include "simple_platformer/navigation/navigation_fill.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/fixed_step.hpp"

namespace tests
{
    // Queues and keeps every cell for each platformer NPC body now, as the game does
    // over the first steps of a level.
    inline void fillNavigation(
        const simple_platformer::TileMap& map,
        simple_platformer::World& world)
    {
        simple_platformer::PlatformerConnectionCache& cache = world.platformerConnections();
        simple_platformer::queueNavigation(
            map, simple_platformer::platformerBodiesIn(world, FixedStepSeconds), cache);
        while (simple_platformer::fillNavigation(
                   map, cache, simple_platformer::NavigationFillTicksPerStep)
                   .cells > 0)
        {
        }
    }
}
