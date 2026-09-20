#pragma once

namespace simple_platformer
{
    class TileMap;
    class World;
    class WorldRequests;

    void updateProjectiles(TileMap& map, World& world, WorldRequests& requests, float deltaTime);

    void updateProjectileBursts(World& world, WorldRequests& requests, float deltaTime);
}
