#include "simple_platformer/world/sight.hpp"

#include <optional>

#include <glm/vec2.hpp>

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
        return map.blocksSight(worldToGrid(map.tileSize(), centerOf(bounds)));
    }

    bool hiddenByCover(const TileMap& map, std::optional<glm::vec2> viewer, const Aabb& target)
    {
        if (!standsInCover(map, target))
        {
            return false;
        }
        return !viewer.has_value() || !lineOfSight(map, *viewer, centerOf(target));
    }
}
