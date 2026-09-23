#pragma once

#include <cstddef>

#include <glm/vec2.hpp>

namespace simple_platformer
{
    // World X increases right and world Y increases down.
    constexpr int InternalWidth = 320;
    constexpr int InternalHeight = 180;
    constexpr glm::vec2 InternalViewportSize = {
        static_cast<float>(InternalWidth),
        static_cast<float>(InternalHeight)};

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

    // For keying an unordered container by cell.
    struct GridPositionHash
    {
        std::size_t operator()(GridPosition cell) const;
    };

    // How many cells a grid has across and down.
    struct GridSize
    {
        int width = 0;
        int height = 0;
    };

    constexpr bool contains(GridSize grid, GridPosition cell)
    {
        return cell.x >= 0 && cell.x < grid.width && cell.y >= 0 && cell.y < grid.height;
    }

    // Points within this of a tile edge count as on the side they visually belong to: feet
    // resting on a tile's top edge stand in the cell above it, a box whose edge lies on a
    // boundary covers only the cells inside it, and a body this close to the map's edge is
    // touching it.
    constexpr float EdgeTolerance = 0.001F;

    // The cell containing the point. A point on a tile edge belongs to the cell to its right
    // or below, and points left of or above the map give negative cells.
    GridPosition worldToGrid(int tileSize, glm::vec2 worldPosition);
    // The cell's top-left corner.
    glm::vec2 gridToWorld(int tileSize, GridPosition cell);

    // The cell something with these feet stands in. Feet exactly on a tile's top edge
    // belong to the cell above it, the one the actor occupies.
    GridPosition cellAtFeet(int tileSize, glm::vec2 feet);
    // The feet of something standing in the cell: the middle of the cell's bottom edge.
    glm::vec2 feetInCell(int tileSize, GridPosition cell);
}
