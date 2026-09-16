#include "simple_platformer/input/input_state.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace
{
    std::size_t indexOf(simple_platformer::InputButton button)
    {
        const std::size_t index = static_cast<std::size_t>(button);
        if (index >= static_cast<std::size_t>(simple_platformer::InputButton::Count))
        {
            throw std::invalid_argument("Input button is invalid");
        }

        return index;
    }

    float axis(bool negative, bool positive)
    {
        return static_cast<float>(positive) - static_cast<float>(negative);
    }
}

namespace simple_platformer
{
    void InputState::setButton(InputButton button, bool down)
    {
        const std::size_t index = indexOf(button);
        if (held[index] == down)
        {
            return;
        }

        held[index] = down;
        pressed[index] = pressed[index] || down;
        released[index] = released[index] || !down;
    }

    void InputState::clearButton(InputButton button)
    {
        const std::size_t index = indexOf(button);
        held[index] = false;
        pressed[index] = false;
        released[index] = false;
    }

    bool InputState::isHeld(InputButton button) const
    {
        return held[indexOf(button)];
    }

    bool InputState::wasPressed(InputButton button) const
    {
        return pressed[indexOf(button)];
    }

    bool InputState::wasReleased(InputButton button) const
    {
        return released[indexOf(button)];
    }

    InputIntentions InputState::consumeIntentions()
    {
        InputIntentions intentions;
        intentions.direction = {
            axis(isHeld(InputButton::Left), isHeld(InputButton::Right)),
            axis(isHeld(InputButton::Up), isHeld(InputButton::Down))};
        intentions.jumpPressed = wasPressed(InputButton::Jump);
        intentions.jumpHeld = isHeld(InputButton::Jump);
        intentions.primaryAttackPressed = wasPressed(InputButton::PrimaryAttack);

        std::fill(pressed.begin(), pressed.end(), false);
        std::fill(released.begin(), released.end(), false);
        return intentions;
    }
}
