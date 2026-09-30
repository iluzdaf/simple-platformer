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

    bool isFinitePositive(glm::vec2 value)
    {
        return isFinitePositive(value.x) && isFinitePositive(value.y);
    }

    bool isFiniteNonNegative(float value)
    {
        return std::isfinite(value) && value >= 0.0F;
    }

    bool isFiniteNonNegative(glm::vec2 value)
    {
        return isFiniteNonNegative(value.x) && isFiniteNonNegative(value.y);
    }

    void requireFinite(glm::vec2 value, const char* what)
    {
        if (!isFinite(value))
        {
            throw std::invalid_argument(std::string(what) + " must be finite");
        }
    }

    void requireSeconds(float seconds, const char* what)
    {
        if (!isFiniteNonNegative(seconds))
        {
            throw std::invalid_argument(
                std::string(what) + " must be a finite, non-negative number of seconds");
        }
    }

    void requirePositiveSeconds(float seconds, const char* what)
    {
        if (!isFinitePositive(seconds))
        {
            throw std::invalid_argument(std::string(what) + " must be finite and positive");
        }
    }
}
