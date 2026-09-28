#pragma once

#include <vector>

#include "simple_platformer/actor/actor_id.hpp"

namespace simple_platformer
{
    class NpcActivityScripts;
    struct FrameProfile;
    class TileMap;
    class World;

    // Chooses each NPC's state and the intentions that act on it, searching the world's
    // navigation for paths as needed. Optional profiling records search work directly.
    void updateNpcBehaviour(
        const TileMap& map,
        World& world,
        float deltaTime,
        NpcActivityScripts* scripts = nullptr,
        FrameProfile* profile = nullptr);

    // Discards script-owned state before queued actor removals are applied to World.
    void forgetNpcActivities(const std::vector<ActorId>& actors, NpcActivityScripts& scripts);
}
