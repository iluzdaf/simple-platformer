#pragma once

#include <glm/vec2.hpp>

#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    struct Actor;
    struct LevelCatalog;
    struct ItemCatalog;

    struct GameLevel
    {
        int number = 0;
        TileMap map;
        World world;
        glm::vec2 playerSpawnFeet = {0.0F, 0.0F};
    };

    // Standalone level construction loads its own item catalogue.
    GameLevel makeGameLevel(const LevelCatalog& catalog, int levelNumber, int textureId);
    // Reuse a session's item identities when carrying inventory between levels.
    GameLevel makeGameLevel(
        const LevelCatalog& catalog,
        int levelNumber,
        int textureId,
        const ItemCatalog& items);
    Actor makePlayer(const LevelCatalog& catalog, int textureId);
}
