#pragma once

namespace simple_platformer
{
    class TileMap;
    class World;

    // Runs one gameplay step and applies queued world changes. A completed world does
    // nothing; while its exit opens, only the world clock and exit check advance.
    void updateWorldSimulation(TileMap& map, World& world, float deltaTime);
}
