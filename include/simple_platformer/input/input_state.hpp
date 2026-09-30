#pragma once

#include <array>
#include <cstddef>

#include <glm/vec2.hpp>

namespace simple_platformer
{
    enum class InputButton
    {
        Left,
        Right,
        Up,
        Down,
        Jump,
        PrimaryAttack,
        Count
    };

    // What an actor with a climb component does with its grip this tick. Keep, the
    // default, leaves it as it is: an actor holding a wall or ceiling stays on, and one
    // that is not holding does not grab. Code with nothing to say about climbing, such as
    // a path follower between steps, leaves it at Keep.
    enum class ClimbGrip
    {
        Keep,
        // Grab a touched wall or ceiling, or stay on the one held.
        Hold,
        Release
    };

    struct InputIntentions
    {
        glm::vec2 direction = {0.0F, 0.0F};
        glm::vec2 aimDirection = {0.0F, 0.0F};
        bool jumpPressed = false;
        bool jumpHeld = false;
        bool primaryAttackPressed = false;
        ClimbGrip climbGrip = ClimbGrip::Keep;
        // Keep grounded walking on its current floor; deliberate jumps still work.
        bool avoidLedges = false;
        // Request body-overlap damage, if the actor has that component.
        bool contactDamage = false;
    };

    class InputState
    {
    public:
        void setButton(InputButton button, bool down);
        void clearButton(InputButton button);

        bool isHeld(InputButton button) const;
        bool wasPressed(InputButton button) const;
        bool wasReleased(InputButton button) const;

        // Pressed and released edges remain pending until a fixed update consumes them.
        InputIntentions consumeIntentions();

    private:
        static constexpr std::size_t ButtonCount = static_cast<std::size_t>(InputButton::Count);

        std::array<bool, ButtonCount> held{};
        std::array<bool, ButtonCount> pressed{};
        std::array<bool, ButtonCount> released{};
    };
}
