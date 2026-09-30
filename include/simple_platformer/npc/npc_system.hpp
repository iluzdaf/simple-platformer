#pragma once

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
}
