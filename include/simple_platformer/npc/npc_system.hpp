#pragma once

namespace simple_platformer
{
    class TileMap;
    class World;
    void updateNpcBehaviour(const TileMap& map, World& world, float deltaTime);
}
