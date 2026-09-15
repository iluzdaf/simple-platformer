#pragma once

namespace simple_platformer
{
    class TileMap;
    class World;

    void updateActorMovement(const TileMap& map, World& world, float deltaTime);
}
