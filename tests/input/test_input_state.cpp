#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "simple_platformer/input/input_state.hpp"

namespace
{
    using simple_platformer::InputButton;
    using simple_platformer::InputIntentions;
    using simple_platformer::InputState;
}

TEST_CASE("Held buttons produce two-dimensional input intentions", "[input]")
{
    InputState input;
    input.setButton(InputButton::Left, true);
    input.setButton(InputButton::Down, true);

    const InputIntentions intentions = input.consumeIntentions();

    REQUIRE(intentions.direction.x == -1.0F);
    REQUIRE(intentions.direction.y == 1.0F);
}

TEST_CASE("Opposing direction buttons cancel each other", "[input]")
{
    InputState input;
    input.setButton(InputButton::Left, true);
    input.setButton(InputButton::Right, true);
    input.setButton(InputButton::Up, true);
    input.setButton(InputButton::Down, true);

    const InputIntentions intentions = input.consumeIntentions();

    REQUIRE(intentions.direction.x == 0.0F);
    REQUIRE(intentions.direction.y == 0.0F);
}

TEST_CASE("A pressed edge is consumed by only one fixed update", "[input]")
{
    InputState input;
    input.setButton(InputButton::Jump, true);

    const InputIntentions firstUpdate = input.consumeIntentions();
    const InputIntentions secondUpdate = input.consumeIntentions();

    REQUIRE(firstUpdate.jumpPressed);
    REQUIRE(firstUpdate.jumpHeld);
    REQUIRE_FALSE(secondUpdate.jumpPressed);
    REQUIRE(secondUpdate.jumpHeld);
}

TEST_CASE("Input edges remain pending until intentions are consumed", "[input]")
{
    InputState input;
    input.setButton(InputButton::PrimaryAttack, true);

    REQUIRE(input.wasPressed(InputButton::PrimaryAttack));
    REQUIRE(input.isHeld(InputButton::PrimaryAttack));

    const InputIntentions intentions = input.consumeIntentions();

    REQUIRE(intentions.primaryAttackPressed);
    REQUIRE_FALSE(input.wasPressed(InputButton::PrimaryAttack));
}

TEST_CASE("Repeated key events do not create extra pressed edges", "[input]")
{
    InputState input;
    input.setButton(InputButton::Jump, true);
    static_cast<void>(input.consumeIntentions());

    input.setButton(InputButton::Jump, true);

    REQUIRE_FALSE(input.consumeIntentions().jumpPressed);
}

TEST_CASE("A quick press and release between fixed updates is preserved", "[input]")
{
    InputState input;
    input.setButton(InputButton::Jump, true);
    input.setButton(InputButton::Jump, false);

    REQUIRE(input.wasPressed(InputButton::Jump));
    REQUIRE(input.wasReleased(InputButton::Jump));

    const InputIntentions intentions = input.consumeIntentions();

    REQUIRE(intentions.jumpPressed);
    REQUIRE_FALSE(intentions.jumpHeld);
    REQUIRE_FALSE(input.wasReleased(InputButton::Jump));
}

TEST_CASE("The sentinel input button is rejected", "[input]")
{
    InputState input;

    REQUIRE_THROWS_AS(input.setButton(InputButton::Count, true), std::invalid_argument);
}
