#pragma once

#include "simple_platformer/timing/fixed_step.hpp"

namespace tests
{
    // The game's fixed simulation step, as the float the update functions take.
    constexpr float FixedStepSeconds = static_cast<float>(simple_platformer::FixedDeltaSeconds);
}
