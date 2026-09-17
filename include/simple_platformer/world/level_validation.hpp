#pragma once

namespace simple_platformer
{
    class TileMap;
    class World;

    // Checks actor spawn positions and patrol endpoints against the level's tile map.
    void validateLevelActors(const TileMap& map, const World& world, int level);
}
