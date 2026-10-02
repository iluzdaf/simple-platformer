#pragma once

namespace simple_platformer
{
    class TileMap;
    class World;
    struct FrameProfile;

    // What one NPC behaviour update works with, shared by the parts of it that act.
    // Profiling is optional.
    struct NpcUpdate
    {
        const TileMap& map;
        World& world;
        float deltaTime;
        FrameProfile* profile;
    };
}
