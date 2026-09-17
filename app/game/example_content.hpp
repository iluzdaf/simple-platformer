#pragma once

namespace simple_platformer
{
    struct Actor;
    class TileMap;
    class World;

    TileMap makeExampleLevel(int level);
    Actor makeExamplePlayer(int textureId);
    void populateExampleLevel(World& world, int level, int textureId);
}
