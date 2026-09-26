#pragma once

#include <vector>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/navigation/path_search.hpp"

namespace simple_platformer
{
    class NpcActivityScripts;
    class TileMap;
    class World;

    // What the NPCs' searches cost in one update, for the step to charge to its profile:
    // how many ran, their statistics summed, and the seconds they took.
    struct NpcBehaviourCost
    {
        int pathSearches = 0;
        PathSearchStatistics searches;
        float searchSeconds = 0.0F;
    };

    // Chooses each NPC's state and the intentions that act on it, searching the world's
    // navigation for paths as needed, and reports what the searches cost.
    NpcBehaviourCost updateNpcBehaviour(
        const TileMap& map,
        World& world,
        float deltaTime,
        NpcActivityScripts* scripts = nullptr);

    // Discards script-owned state before queued actor removals are applied to World.
    void forgetNpcActivities(const std::vector<ActorId>& actors, NpcActivityScripts& scripts);
}
