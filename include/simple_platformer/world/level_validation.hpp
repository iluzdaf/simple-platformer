#pragma once

namespace simple_platformer
{
    class TileMap;
    class World;

    // Checks actor spawns, the player respawn, and patrol endpoints against the tile map.
    void validateLevelActors(const TileMap& map, const World& world, int level);
}
