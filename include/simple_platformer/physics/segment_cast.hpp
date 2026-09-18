#pragma once

#include <optional>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"

namespace simple_platformer
{
    class TileMap;

    // Returns the fraction along the segment where it first touches or enters the box:
    // 0 means start, 1 means end. Returns nullopt if the segment misses the box.
    std::optional<float> segmentCast(const Aabb& box, glm::vec2 start, glm::vec2 end);

    // Returns the earliest hit fraction (0 to 1) against movement-blocking tiles
    // or map boundaries, or nullopt if clear. Start and end are the moving box's
    // centre positions; a zero size casts a line.
    std::optional<float> segmentCastMovementBlockingTiles(
        const TileMap& map,
        glm::vec2 start,
        glm::vec2 end,
        glm::vec2 movingSize = {0.0F, 0.0F});

    // Returns the earliest hit fraction (0 to 1) along a line against sight-blocking
    // tiles or map boundaries, or nullopt if clear.
    std::optional<float> segmentCastSightBlockingTiles(
        const TileMap& map,
        glm::vec2 start,
        glm::vec2 end);
}
