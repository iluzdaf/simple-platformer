#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include <glm/vec2.hpp>

#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    struct Actor;
    struct LevelCatalog;
    struct GameCatalogs;

    // Runtime map and world, composed from LevelData and shared definitions.
    struct GameLevel
    {
        int number = 0;
        TileMap map;
        World world;
        glm::vec2 playerSpawnFeet = {0.0F, 0.0F};
        // Content names stay outside core actors; debug views resolve them by ID.
        std::unordered_map<std::uint32_t, std::string> actorDefinitionNames;
    };

    // Game::startLevel inserts the player; only the requested level file is read here.
    GameLevel composeGameLevel(
        const LevelCatalog& catalog,
        int levelNumber,
        int textureId,
        const GameCatalogs& catalogs);
    Actor composePlayer(const GameCatalogs& catalogs, int textureId);
}
