#pragma once

#include <cstdint>
#include <optional>

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    struct ActorId
    {
        std::uint32_t value = 0;
    };

    bool operator==(ActorId left, ActorId right);
    bool operator!=(ActorId left, ActorId right);
    bool isValid(ActorId id);

    struct Health
    {
        int current = 1;
        int maximum = 1;
    };

    enum class LifeState
    {
        Alive,
        Dying
    };

    struct Actor
    {
        ActorId id;
        Body body;
        InputIntentions intentions;
        std::optional<PlatformerMovement> platformerMovement;
        Facing facing = Facing::Right;

        LifeState life = LifeState::Alive;
        float deathTimeRemaining = 0.0F;

        std::optional<Sprite> sprite;
        std::optional<Animator> animator;
        std::optional<Health> health;
    };
}
