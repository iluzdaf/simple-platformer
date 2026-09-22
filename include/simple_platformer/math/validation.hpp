#pragma once

#include <glm/vec2.hpp>

namespace simple_platformer
{
    bool isFinite(glm::vec2 value);
    bool isFinitePositive(float value);

    // A length of time is finite and not negative; zero is allowed. The message starts with
    // what was measured, as in "Frame time".
    void requireSeconds(float seconds, const char* what);

    // The same rule for an update's time step; a zero step advances nothing. The message
    // starts with what was being updated, as in "Attacks".
    void requireTimeStep(float deltaTime, const char* what);
}
