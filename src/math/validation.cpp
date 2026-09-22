#include "simple_platformer/math/validation.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

#include <glm/vec2.hpp>

namespace simple_platformer
{
    bool isFinite(glm::vec2 value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y);
    }

    void requireTimeStep(float deltaTime, const char* what)
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0F)
        {
            throw std::invalid_argument(
                std::string(what) + " require a finite, non-negative time step");
        }
    }
}
