#pragma once

#include <glm/vec2.hpp>

namespace simple_platformer
{
    // Non-throwing checks for validators that combine rules or supply their own messages.
    bool isFinite(glm::vec2 value);
    bool isFinitePositive(float value);
    // Both coordinates must be finite and greater than zero, as required for sizes.
    bool isFinitePositive(glm::vec2 value);
    bool isFiniteNonNegative(float value);
    // Both coordinates must be finite and non-negative, as required for atlas positions.
    bool isFiniteNonNegative(glm::vec2 value);

    // Throws unless x and y are both finite. The message starts with what the value is, as
    // in "Input intentions must be finite".
    void requireFinite(glm::vec2 value, const char* what);

    // Requires finite, non-negative seconds. Zero is allowed. The error begins with what,
    // such as "Frame time" or "Attacks time step".
    void requireSeconds(float seconds, const char* what);

    // Requires finite, positive seconds for simulations that cannot use a zero step.
    void requirePositiveSeconds(float seconds, const char* what);
}
