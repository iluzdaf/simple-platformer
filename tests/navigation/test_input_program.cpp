#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/navigation/input_program.hpp"

TEST_CASE("An input program replays consecutive intentions", "[navigation][input-program]")
{
    simple_platformer::InputIntentions jump;
    jump.jumpPressed = true;
    jump.jumpHeld = true;
    simple_platformer::InputIntentions right;
    right.direction.x = 1.0F;
    const simple_platformer::InputProgram program{{0.1F, jump}, {0.2F, right}};

    REQUIRE(simple_platformer::durationOf(program) == 0.3F);
    REQUIRE(simple_platformer::replayInput(program, 0.0F).jumpPressed);
    REQUIRE(simple_platformer::replayInput(program, 0.1F).direction.x == 1.0F);
    REQUIRE(simple_platformer::replayInput(program, 0.3F).direction.x == 0.0F);
}

TEST_CASE("Input programs reject invalid time", "[navigation][input-program]")
{
    REQUIRE_THROWS_AS(simple_platformer::durationOf({{0.0F, {}}}), std::invalid_argument);
    REQUIRE_THROWS_AS(simple_platformer::replayInput({}, -0.1F), std::invalid_argument);
}
