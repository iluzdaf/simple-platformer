#include "simple_platformer/world/sight.hpp"

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/physics/segment_cast.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    bool lineOfSight(const TileMap& map, glm::vec2 from, glm::vec2 to)
    {
        return !segmentCastSightBlockingTiles(map, from, to).has_value();
    }

    bool standsInCover(const TileMap& map, const Aabb& bounds)
    {
        return map.blocksSight(worldToGrid(centerOf(bounds)));
    }

    bool hiddenByCover(const TileMap& map, const Actor* viewer, const Aabb& target)
    {
        if (!standsInCover(map, target))
        {
            return false;
        }
        return viewer == nullptr ||
               !lineOfSight(map, centerOf(viewer->body.bounds), centerOf(target));
    }
}
