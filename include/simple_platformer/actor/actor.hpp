#pragma once

#include <optional>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/inventory/inventory.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
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
        std::optional<FlyingMovement> flyingMovement;
        std::optional<SurfaceClimb> surfaceClimb;
        Facing facing = Facing::Right;

        LifeState life = LifeState::Alive;
        float deathTimeRemaining = 0.0F;
        std::optional<double> lastDamageTimeSeconds;

        std::optional<Sprite> sprite;
        std::optional<Animator> animator;
        // Screen visibility from 0 to 1, eased by cover fading. The player uses it for
        // shading; other actors use it for opacity. Unset until first presented.
        std::optional<float> screenVisibility;
        std::optional<Health> health;
        std::optional<Inventory> inventory;
        Team team = Team::Neutral;
        std::optional<RangedWeapon> rangedWeapon;
        std::optional<BiteAttack> bite;
        std::optional<ContactDamage> contactDamage;
        std::optional<NpcBrain> brain;
        std::optional<NpcPerception> perception;
        std::optional<NpcSenses> senses;
        std::optional<Patrol> patrol;
        std::optional<PathFollower> pathFollower;
    };
}
