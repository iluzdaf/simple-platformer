#pragma once

#include <glm/vec2.hpp>

namespace simple_platformer
{
    // World X increases right and world Y increases down.
    constexpr int InternalWidth = 320;
    constexpr int InternalHeight = 180;
    constexpr int TileSize = 16;

    struct GridPosition
    {
        int x = 0;
        int y = 0;
    };

    constexpr bool operator==(GridPosition left, GridPosition right)
    {
        return left.x == right.x && left.y == right.y;
    }

    constexpr bool operator!=(GridPosition left, GridPosition right)
    {
        return !(left == right);
    }

    GridPosition worldToGrid(glm::vec2 worldPosition);
    glm::vec2 gridToWorld(GridPosition gridPosition);
}
