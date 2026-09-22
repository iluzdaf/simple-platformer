#pragma once

#include <glm/vec2.hpp>

namespace simple_platformer
{
    // World X increases right and world Y increases down.
    constexpr int InternalWidth = 320;
    constexpr int InternalHeight = 180;

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

    // The cell containing the point. A point on a tile edge belongs to the cell to its right
    // or below, and points left of or above the map give negative cells.
    GridPosition worldToGrid(int tileSize, glm::vec2 worldPosition);
    // The cell's top-left corner.
    glm::vec2 gridToWorld(int tileSize, GridPosition gridPosition);

    // The cell something with these feet stands in. Feet exactly on a tile's top edge
    // belong to the cell above it, the one the actor occupies.
    GridPosition cellAtFeet(int tileSize, glm::vec2 feet);
    // The feet of something standing in the cell: the middle of the cell's bottom edge.
    glm::vec2 feetInCell(int tileSize, GridPosition cell);
}
