#include "simple_platformer/world/sight.hpp"

#include <glm/vec2.hpp>

#include "simple_platformer/physics/segment_cast.hpp"

namespace simple_platformer
{
    bool lineOfSight(const TileMap& map, glm::vec2 from, glm::vec2 to)
    {
        return !segmentCastSightBlockingTiles(map, from, to).has_value();
    }
}
