#pragma once

#include <optional>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"

namespace simple_platformer
{
    // Returns the first point along start-to-end that enters the box, from 0 through 1.
    std::optional<float> segmentCast(const Aabb& box, glm::vec2 start, glm::vec2 end);
}
