#pragma once

#include <glm/vec2.hpp>

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
}
