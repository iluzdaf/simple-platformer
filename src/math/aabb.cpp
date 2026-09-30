#include "simple_platformer/math/aabb.hpp"

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"

namespace simple_platformer
{
    float rightOf(const Aabb& box)
    {
        return box.position.x + box.size.x;
    }

    float bottomOf(const Aabb& box)
    {
        return box.position.y + box.size.y;
    }

    glm::vec2 centerOf(const Aabb& box)
    {
        return box.position + box.size * 0.5F;
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
            cellAt(tileSize, {box.position.x + EdgeTolerance, box.position.y + EdgeTolerance}),
            cellAt(tileSize, {rightOf(box) - EdgeTolerance, bottomOf(box) - EdgeTolerance})};
    }

    bool overlaps(const Aabb& first, const Aabb& second)
    {
        return first.position.x < rightOf(second) && rightOf(first) > second.position.x &&
               first.position.y < bottomOf(second) && bottomOf(first) > second.position.y;
    }

    bool contains(const Aabb& box, glm::vec2 point)
    {
        return point.x >= box.position.x && point.y >= box.position.y && point.x < rightOf(box) &&
               point.y < bottomOf(box);
    }
}
