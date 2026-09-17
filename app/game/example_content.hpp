#pragma once

#include <glm/vec2.hpp>

#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    struct Actor;
    struct LevelCatalog;

    struct GameLevel
    {
        int number = 0;
        TileMap map;
        World world;
        glm::vec2 playerSpawnFeet = {0.0F, 0.0F};
    };

    GameLevel makeGameLevel(const LevelCatalog& catalog, int levelNumber, int textureId);
    Actor makeExamplePlayer(int textureId);
}
