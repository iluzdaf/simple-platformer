#include "simple_platformer/math/validation.hpp"

#include <cmath>

#include <glm/vec2.hpp>

namespace simple_platformer
{
    bool isFinite(glm::vec2 value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y);
    }
}
