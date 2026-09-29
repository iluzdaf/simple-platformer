#include "simple_platformer/math/coordinates.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace simple_platformer
{
    std::size_t CellHash::operator()(Cell cell) const
    {
        const auto column = static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell.x));
        const auto row = static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell.y));
        return std::hash<std::uint64_t>{}((column << 32U) | row);
    }

    bool contains(const CellRange& range, Cell cell)
    {
        return cell.x >= range.first.x && cell.x <= range.last.x && cell.y >= range.first.y &&
               cell.y <= range.last.y;
    }

    CellRange unionOf(const CellRange& left, const CellRange& right)
    {
        return {
            {std::min(left.first.x, right.first.x), std::min(left.first.y, right.first.y)},
            {std::max(left.last.x, right.last.x), std::max(left.last.y, right.last.y)}};
    }

    Cell cellAt(int tileSize, glm::vec2 worldPosition)
    {
        return {
            static_cast<int>(std::floor(worldPosition.x / static_cast<float>(tileSize))),
            static_cast<int>(std::floor(worldPosition.y / static_cast<float>(tileSize))),
        };
    }

    glm::vec2 cellCorner(int tileSize, Cell cell)
    {
        return {
            static_cast<float>(cell.x * tileSize),
            static_cast<float>(cell.y * tileSize),
        };
    }

    Cell cellAtFeet(int tileSize, glm::vec2 feet)
    {
        return cellAt(tileSize, {feet.x, feet.y - EdgeTolerance});
    }

    glm::vec2 feetInCell(int tileSize, Cell cell)
    {
        const glm::vec2 topLeft = cellCorner(tileSize, cell);
        return {
            topLeft.x + static_cast<float>(tileSize) * 0.5F,
            topLeft.y + static_cast<float>(tileSize)};
    }
}
