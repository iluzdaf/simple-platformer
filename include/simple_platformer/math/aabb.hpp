#pragma once

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"

namespace simple_platformer
{
    struct Aabb
    {
        // Top-left position and dimensions measured in world pixels.
        glm::vec2 position = {0.0F, 0.0F};
        glm::vec2 size = {0.0F, 0.0F};
    };

    glm::vec2 centerOf(const Aabb& box);
    glm::vec2 feetOf(const Aabb& box);
    void placeFeetAt(Aabb& box, glm::vec2 feet);
    // A box of this size standing in the cell, its feet at the middle of the cell's bottom edge.
    Aabb boxInCell(int tileSize, GridPosition cell, glm::vec2 size);

    struct CellRange
    {
        GridPosition first;
        // Inclusive.
        GridPosition last;
    };

    // The cells the box lies over. Its edges are read EdgeTolerance inside, so a box resting
    // exactly on a boundary does not also cover the cell beyond it.
    CellRange cellsCovered(int tileSize, const Aabb& box);
    // Edge contact alone is not an overlap.
    bool overlaps(const Aabb& first, const Aabb& second);
}
