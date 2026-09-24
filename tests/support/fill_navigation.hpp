#pragma once

#include "simple_platformer/navigation/navigation_fill.hpp"
#include "support/fixed_step.hpp"

namespace tests
{
    // Queues and keeps every cell for each platformer NPC body now, as the game does
    // over the first steps of a level.
    inline void fillNavigation(
        const simple_platformer::TileMap& map,
        simple_platformer::World& world)
    {
        simple_platformer::queueWorldNavigation(map, world, FixedStepSeconds);
        while (simple_platformer::fillWorldNavigation(map, world, FixedStepSeconds).cells > 0)
        {
        }
    }
}
