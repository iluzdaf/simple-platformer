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
}
