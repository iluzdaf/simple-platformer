#pragma once

#include <glm/vec2.hpp>

namespace simple_platformer
{
    // Predicates for validators that combine conditions or word their own message.
    bool isFinite(glm::vec2 value);
    bool isFinitePositive(float value);
    // Both parts, as a size's must be.
    bool isFinitePositive(glm::vec2 value);
    bool isFiniteNonNegative(float value);

    // Throws that the value "must be finite", after what it is, as in "Input intentions".
    void requireFinite(glm::vec2 value, const char* what);

    // A length of time is finite and not negative; zero is allowed, and for an update's
    // time step it advances nothing. The message starts with what the seconds are, as in
    // "Frame time" or "Attacks time step".
    void requireSeconds(float seconds, const char* what);

    // Simulation steps that cannot be zero use this stricter check.
    void requirePositiveSeconds(float seconds, const char* what);
}
