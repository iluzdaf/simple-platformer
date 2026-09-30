#include "simple_platformer/math/aabb.hpp"

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"

namespace simple_platformer
{
    float rightOf(const Aabb& box)
    {
        return box.topLeft.x + box.size.x;
    }

    float bottomOf(const Aabb& box)
    {
        return box.topLeft.y + box.size.y;
    }

    glm::vec2 centerOf(const Aabb& box)
    {
        return box.topLeft + box.size * 0.5F;
    }

    glm::vec2 topCenterOf(const Aabb& box)
    {
        return {centerOf(box).x, box.topLeft.y};
    }

    glm::vec2 feetOf(const Aabb& box)
    {
        return {centerOf(box).x, bottomOf(box)};
    }

    Aabb boxCenteredOn(glm::vec2 center, glm::vec2 size)
    {
        return {center - size * 0.5F, size};
    }

    Aabb boxStandingOn(glm::vec2 feet, glm::vec2 size)
    {
        return {{feet.x - size.x * 0.5F, feet.y - size.y}, size};
    }

    void moveFeetTo(Aabb& box, glm::vec2 feet)
    {
        box = boxStandingOn(feet, box.size);
    }

    Aabb boxInCell(int tileSize, Cell cell, glm::vec2 size)
    {
        return boxStandingOn(feetInCell(tileSize, cell), size);
    }

    CellRange cellsCovered(int tileSize, const Aabb& box)
    {
        return {
            cellAt(tileSize, {box.topLeft.x + EdgeTolerance, box.topLeft.y + EdgeTolerance}),
            cellAt(tileSize, {rightOf(box) - EdgeTolerance, bottomOf(box) - EdgeTolerance})};
    }

    bool overlaps(const Aabb& first, const Aabb& second)
    {
        return first.topLeft.x < rightOf(second) && rightOf(first) > second.topLeft.x &&
               first.topLeft.y < bottomOf(second) && bottomOf(first) > second.topLeft.y;
    }

    bool contains(const Aabb& box, glm::vec2 point)
    {
        return point.x >= box.topLeft.x && point.y >= box.topLeft.y && point.x < rightOf(box) &&
               point.y < bottomOf(box);
    }
}
