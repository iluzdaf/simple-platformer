#pragma once

namespace simple_platformer
{
    class NpcActivityScripts;
    class TileMap;
    class World;
    struct FrameProfile;

    // What one NPC behaviour update works with, shared by the parts of it that act.
    // Scripts and profile are optional.
    struct NpcUpdate
    {
        const TileMap& map;
        World& world;
        float deltaTime;
        NpcActivityScripts* scripts;
        FrameProfile* profile;
    };
}
