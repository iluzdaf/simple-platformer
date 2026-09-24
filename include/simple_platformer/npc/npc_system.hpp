#pragma once

#include "simple_platformer/navigation/path_search.hpp"

namespace simple_platformer
{
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
    // navigation for paths as needed. With a cost, reports what the searches cost;
    // without one, no clock is read.
    void updateNpcBehaviour(
        const TileMap& map,
        World& world,
        float deltaTime,
        NpcBehaviourCost* cost = nullptr);
}
