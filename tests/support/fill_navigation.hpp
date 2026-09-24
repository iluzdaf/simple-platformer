#pragma once

#include "simple_platformer/npc/npc_system.hpp"
#include "support/fixed_step.hpp"

namespace tests
{
    // Queues and keeps every cell for each platformer NPC body now, as the game does
    // over the first steps of a level.
    inline void fillNavigation(
        const simple_platformer::TileMap& map,
        simple_platformer::World& world)
    {
        simple_platformer::queueNpcNavigation(map, world, FixedStepSeconds);
        while (simple_platformer::fillNpcNavigation(map, world, FixedStepSeconds).cells > 0)
        {
        }
    }
}
