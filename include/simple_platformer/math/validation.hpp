#pragma once

#include <glm/vec2.hpp>

namespace simple_platformer
{
    bool isFinite(glm::vec2 value);

    // Every update takes a time step that is finite and not negative. Zero is allowed: it
    // advances nothing. The message starts with what was being updated, as in "Attacks".
    void requireTimeStep(float deltaTime, const char* what);
}
