#include "simple_platformer/math/coordinates.hpp"

#include <cmath>

namespace simple_platformer
{
    GridPosition worldToGrid(int tileSize, glm::vec2 worldPosition)
    {
        return {
            static_cast<int>(std::floor(worldPosition.x / static_cast<float>(tileSize))),
            static_cast<int>(std::floor(worldPosition.y / static_cast<float>(tileSize))),
        };
    }

    glm::vec2 gridToWorld(int tileSize, GridPosition cell)
    {
        return {
            static_cast<float>(cell.x * tileSize),
            static_cast<float>(cell.y * tileSize),
        };
    }

    GridPosition cellAtFeet(int tileSize, glm::vec2 feet)
    {
        constexpr float BoundaryOffset = 0.001F;
        return worldToGrid(tileSize, {feet.x, feet.y - BoundaryOffset});
    }

    glm::vec2 feetInCell(int tileSize, GridPosition cell)
    {
        const glm::vec2 topLeft = gridToWorld(tileSize, cell);
        return {
            topLeft.x + static_cast<float>(tileSize) * 0.5F,
            topLeft.y + static_cast<float>(tileSize)};
    }
}
