#pragma once

#include <optional>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"

namespace simple_platformer
{
    class TileMap;

    // Every cast reports how far along the segment the hit happened, as a fraction where
    // 0 is the start and 1 is the end, or nullopt when nothing is hit. The tile casts
    // treat map boundaries as blocking, so a reported cell can lie outside the map.

    // Touching the box counts as a hit.
    std::optional<float> segmentCast(const Aabb& box, glm::vec2 start, glm::vec2 end);

    struct TileSegmentHit
    {
        float segmentTime = 0.0F;
        GridPosition cell;
    };

    // Earliest movement-blocking tile. Start and end are the moving box's centre
    // positions; a zero size casts a line.
    std::optional<TileSegmentHit> segmentCastMovementBlockingTiles(
        const TileMap& map,
        glm::vec2 start,
        glm::vec2 end,
        glm::vec2 movingSize = {0.0F, 0.0F});

    // Earliest sight-blocking tile along a line.
    std::optional<float> segmentCastSightBlockingTiles(
        const TileMap& map,
        glm::vec2 start,
        glm::vec2 end);
}
