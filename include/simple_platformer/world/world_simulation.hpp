#pragma once

namespace simple_platformer
{
    class TileMap;
    class World;

    // One fixed step of gameplay.
    void updateWorldSimulation(TileMap& map, World& world, float deltaTime);
}
