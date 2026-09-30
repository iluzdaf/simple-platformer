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

    // The far edges. The near ones, left and top, are the position.
    float rightOf(const Aabb& box);
    float bottomOf(const Aabb& box);

    glm::vec2 centerOf(const Aabb& box);
    // Its feet: the middle of its bottom edge, where a standing body meets the ground.
    glm::vec2 feetOf(const Aabb& box);

    // A box of this size whose centre is the point.
    Aabb boxCenteredOn(glm::vec2 center, glm::vec2 size);
    // A box of this size whose feet are the point.
    Aabb boxStandingOn(glm::vec2 feet, glm::vec2 size);
    // Moves the box, keeping its size, so its feet are the point.
    void moveFeetTo(Aabb& box, glm::vec2 feet);
    // A box of this size standing in the cell, its feet at the middle of the cell's bottom edge.
    Aabb boxInCell(int tileSize, Cell cell, glm::vec2 size);

    // The cells the box lies over. Its edges are read EdgeTolerance inside, so a box resting
    // exactly on a boundary does not also cover the cell beyond it.
    CellRange cellsCovered(int tileSize, const Aabb& box);
    // Edge contact alone is not an overlap.
    bool overlaps(const Aabb& first, const Aabb& second);
    // A point on the left or top edge is inside; one on the right or bottom edge is not.
    bool contains(const Aabb& box, glm::vec2 point);
}
