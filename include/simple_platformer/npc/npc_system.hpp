#pragma once

#include <vector>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/navigation/path_search.hpp"

namespace simple_platformer
{
    class NpcActivityScripts;
    struct FrameProfile;
    class TileMap;
    class World;

    // What the NPCs' searches did in one update, for the step's counters.
    struct NpcBehaviourStatistics
    {
        int pathSearches = 0;
        PathSearchStatistics searches;
    };

    // Chooses each NPC's state and the intentions that act on it, searching the world's
    // navigation for paths as needed, and reports what the searches cost.
    NpcBehaviourStatistics updateNpcBehaviour(
        const TileMap& map,
        World& world,
        float deltaTime,
        NpcActivityScripts* scripts = nullptr,
        FrameProfile* profile = nullptr);

    // Discards script-owned state before queued actor removals are applied to World.
    void forgetNpcActivities(const std::vector<ActorId>& actors, NpcActivityScripts& scripts);
}
