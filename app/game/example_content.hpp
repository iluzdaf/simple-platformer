#pragma once

#include <glm/vec2.hpp>

#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    struct Actor;
    struct LevelCatalog;
    struct GameCatalogs;

    struct GameLevel
    {
        int number = 0;
        TileMap map;
        World world;
        glm::vec2 playerSpawnFeet = {0.0F, 0.0F};
    };

    // Standalone level construction loads its own shared catalogues.
    GameLevel makeGameLevel(const LevelCatalog& catalog, int levelNumber, int textureId);
    // Reuse the session's definitions; only the requested level file is read here.
    GameLevel makeGameLevel(
        const LevelCatalog& catalog,
        int levelNumber,
        int textureId,
        const GameCatalogs& catalogs);
    Actor makePlayer(const GameCatalogs& catalogs, int textureId);
}
