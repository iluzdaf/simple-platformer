#pragma once

#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    struct Actor;

    struct ExampleLevel
    {
        int number = 0;
        TileMap map;
        World world;
    };

    ExampleLevel makeExampleLevel(int levelNumber, int textureId);
    Actor makeExamplePlayer(int textureId);
}
