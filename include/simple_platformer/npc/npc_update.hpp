#pragma once

namespace simple_platformer
{
    class TileMap;
    class World;

    // What one NPC behaviour update works with, shared by the parts of it that act.
    struct NpcUpdate
    {
        const TileMap& map;
        World& world;
        float deltaTime;
    };
}
