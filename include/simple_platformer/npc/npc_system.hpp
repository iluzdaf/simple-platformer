#pragma once

namespace simple_platformer
{
    class TileMap;
    class World;

    // Chooses each NPC's state and the intentions that act on it, searching the world's
    // navigation for paths as needed.
    void updateNpcBehaviour(const TileMap& map, World& world, float deltaTime);
}
