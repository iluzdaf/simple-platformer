#include "simple_platformer/world/sight.hpp"

#include <algorithm>
#include <optional>

#include <glm/common.hpp>
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

    float fractionInCover(const TileMap& map, const Aabb& bounds)
    {
        const float area = bounds.size.x * bounds.size.y;
        if (area <= 0.0F)
        {
            return 0.0F;
        }

        const glm::vec2 boundsEnd = bounds.position + bounds.size;
        const GridPosition first = worldToGrid(map.tileSize(), bounds.position);
        const GridPosition last = worldToGrid(map.tileSize(), boundsEnd);
        float coveredArea = 0.0F;
        for (int row = first.y; row <= last.y; ++row)
        {
            for (int column = first.x; column <= last.x; ++column)
            {
                if (!map.blocksSight({column, row}))
                {
                    continue;
                }
                const glm::vec2 cellStart = gridToWorld(map.tileSize(), {column, row});
                const glm::vec2 cellEnd = cellStart + static_cast<float>(map.tileSize());
                const glm::vec2 overlap = glm::max(
                    glm::min(boundsEnd, cellEnd) - glm::max(bounds.position, cellStart),
                    glm::vec2{0.0F, 0.0F});
                coveredArea += overlap.x * overlap.y;
            }
        }
        return std::min(coveredArea / area, 1.0F);
    }

    float visibility(
        const TileMap& map,
        std::optional<glm::vec2> viewer,
        const Aabb& target,
        CoverFade fade)
    {
        const float covered = fractionInCover(map, target);
        if (covered <= fade.concealsAbove)
        {
            return 1.0F;
        }
        if (viewer.has_value() && lineOfSight(map, *viewer, centerOf(target)))
        {
            return 1.0F;
        }
        return std::clamp(
            (fade.hidesAbove - covered) / (fade.hidesAbove - fade.concealsAbove), 0.0F, 1.0F);
    }
}
