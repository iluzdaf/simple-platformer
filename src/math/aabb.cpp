#include "simple_platformer/math/aabb.hpp"

namespace simple_platformer
{
    glm::vec2 centerOf(const Aabb& box)
    {
        return box.position + box.size * 0.5F;
    }

    glm::vec2 feetOf(const Aabb& box)
    {
        return {box.position.x + box.size.x * 0.5F, box.position.y + box.size.y};
    }

    void placeFeetAt(Aabb& box, glm::vec2 feet)
    {
        box.position = {feet.x - box.size.x * 0.5F, feet.y - box.size.y};
    }

    bool overlaps(const Aabb& first, const Aabb& second)
    {
        return first.position.x < second.position.x + second.size.x &&
               first.position.x + first.size.x > second.position.x &&
               first.position.y < second.position.y + second.size.y &&
               first.position.y + first.size.y > second.position.y;
    }
}
