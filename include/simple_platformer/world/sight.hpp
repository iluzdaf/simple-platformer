#pragma once

#include <glm/vec2.hpp>

namespace simple_platformer
{
    class TileMap;
    struct Actor;
    struct Aabb;

    // Whether the sight cast from one point reaches the other: nothing that blocks sight is
    // in the way, apart from the cover the line starts in.
    bool lineOfSight(const TileMap& map, glm::vec2 from, glm::vec2 to);

    // Whether the centre of these bounds is in a sight-blocking tile. Only tiles that can be
    // walked into can hold anything, so this is standing in grass rather than in stone.
    bool standsInCover(const TileMap& map, const Aabb& bounds);

    // Whether cover hides the target from the viewer: it stands in cover and the viewer, if
    // there is one, has no line of sight to it. Walls alone never hide a target in the open,
    // and without a viewer nothing in cover is seen.
    bool hiddenByCover(const TileMap& map, const Actor* viewer, const Aabb& target);
}
