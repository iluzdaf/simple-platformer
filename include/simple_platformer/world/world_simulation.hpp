#pragma once

namespace simple_platformer
{
    class TileMap;
    class World;

    void updateWorldSimulation(TileMap& map, World& world, float deltaTime);
}
