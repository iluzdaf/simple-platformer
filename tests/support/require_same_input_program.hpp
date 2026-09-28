#pragma once

#include <cstddef>

#include <catch2/catch_test_macros.hpp>

#include "simple_platformer/input/input_program.hpp"

namespace tests
{
    inline void requireSameInputProgram(
        const simple_platformer::InputProgram& left,
        const simple_platformer::InputProgram& right)
    {
        REQUIRE(left.size() == right.size());
        for (std::size_t index = 0; index < left.size(); ++index)
        {
            const simple_platformer::InputStep& leftStep = left[index];
            const simple_platformer::InputStep& rightStep = right[index];
            REQUIRE(leftStep.duration == rightStep.duration);
            REQUIRE(leftStep.intentions.direction == rightStep.intentions.direction);
            REQUIRE(leftStep.intentions.aimDirection == rightStep.intentions.aimDirection);
            REQUIRE(leftStep.intentions.jumpPressed == rightStep.intentions.jumpPressed);
            REQUIRE(leftStep.intentions.jumpHeld == rightStep.intentions.jumpHeld);
            REQUIRE(
                leftStep.intentions.primaryAttackPressed ==
                rightStep.intentions.primaryAttackPressed);
            REQUIRE(leftStep.intentions.avoidLedges == rightStep.intentions.avoidLedges);
            REQUIRE(leftStep.intentions.contactDamage == rightStep.intentions.contactDamage);
        }
    }
}
