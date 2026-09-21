#pragma once

namespace simple_platformer
{
    class TileMap;
    struct Actor;
    struct Aabb;

    // Whether the player can see something with these bounds. Grass hides what stands in it:
    // something whose centre is in a sight-blocking tile is visible only when a sight cast
    // from the player reaches it, which ignores the cover the player stands in. Anything
    // else is always visible. Without a player, nobody sees into grass.
    bool playerCanSee(const TileMap& map, const Actor* player, const Aabb& bounds);
}
