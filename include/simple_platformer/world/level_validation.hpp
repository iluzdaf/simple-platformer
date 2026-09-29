#pragma once

namespace simple_platformer
{
    class TileMap;
    class World;

    // Checks actor spawns, the player respawn, and patrol endpoints against the tile map.
    // Each needs clearance. A platformer's spawn and respawn also need ground support,
    // and so do its patrol endpoints unless it can climb.
    void validateLevelActors(const TileMap& map, const World& world, int level);
}
