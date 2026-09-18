#pragma once

#include <optional>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"

namespace simple_platformer
{
    class TileMap;

    // Returns the first point along start-to-end that enters the box, from 0 through 1.
    std::optional<float> segmentCast(const Aabb& box, glm::vec2 start, glm::vec2 end);

    // Returns the first solid tile hit by a moving box. A zero size casts a line.
    std::optional<float> segmentCastSolidTiles(
        const TileMap& map,
        glm::vec2 start,
        glm::vec2 end,
        glm::vec2 movingSize = {0.0F, 0.0F});
}
