#pragma once

#include <optional>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
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
        Team team = Team::Neutral;
        std::optional<RangedWeapon> rangedWeapon;
        std::optional<BiteAttack> bite;
    };
}
