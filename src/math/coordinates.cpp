#include "simple_platformer/math/coordinates.hpp"

#include <cmath>

namespace simple_platformer
{
    GridPosition worldToGrid(glm::vec2 worldPosition)
    {
        return {
            static_cast<int>(std::floor(worldPosition.x / static_cast<float>(TileSize))),
            static_cast<int>(std::floor(worldPosition.y / static_cast<float>(TileSize))),
        };
    }

    glm::vec2 gridToWorld(GridPosition gridPosition)
    {
        return {
            static_cast<float>(gridPosition.x * TileSize),
            static_cast<float>(gridPosition.y * TileSize),
        };
    }
}
