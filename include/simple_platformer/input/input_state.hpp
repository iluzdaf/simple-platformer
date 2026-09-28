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

    struct InputIntentions
    {
        glm::vec2 direction = {0.0F, 0.0F};
        glm::vec2 aimDirection = {0.0F, 0.0F};
        bool jumpPressed = false;
        bool jumpHeld = false;
        bool primaryAttackPressed = false;
        // Hold an adjacent wall or ceiling when the actor has a climb component.
        bool climbRequested = false;
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
