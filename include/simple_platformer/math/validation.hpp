#pragma once

#include <glm/vec2.hpp>

namespace simple_platformer
{
    bool isFinite(glm::vec2 value);
    bool isFinitePositive(float value);

    // A length of time is finite and not negative; zero is allowed, and for an update's
    // time step it advances nothing. The message starts with what the seconds are, as in
    // "Frame time" or "Attacks time step".
    void requireSeconds(float seconds, const char* what);
}
