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

    bool isFinitePositive(float value)
    {
        return std::isfinite(value) && value > 0.0F;
    }

    void requireSeconds(float seconds, const char* what)
    {
        if (!std::isfinite(seconds) || seconds < 0.0F)
        {
            throw std::invalid_argument(
                std::string(what) + " must be a finite, non-negative number of seconds");
        }
    }

    void requireTimeStep(float deltaTime, const char* what)
    {
        requireSeconds(deltaTime, (std::string(what) + " time step").c_str());
    }
}
