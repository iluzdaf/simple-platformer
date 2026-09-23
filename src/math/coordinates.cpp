#include "simple_platformer/math/coordinates.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace simple_platformer
{
    std::size_t GridPositionHash::operator()(GridPosition cell) const
    {
        const auto column = static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell.x));
        const auto row = static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell.y));
        return std::hash<std::uint64_t>{}((column << 32U) | row);
    }

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
        return worldToGrid(tileSize, {feet.x, feet.y - EdgeTolerance});
    }

    glm::vec2 feetInCell(int tileSize, GridPosition cell)
    {
        const glm::vec2 topLeft = gridToWorld(tileSize, cell);
        return {
            topLeft.x + static_cast<float>(tileSize) * 0.5F,
            topLeft.y + static_cast<float>(tileSize)};
    }
}
