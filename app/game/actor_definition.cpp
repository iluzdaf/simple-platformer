#include "actor_definition.hpp"
#include "example_animations.hpp"
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include "simple_platformer/actor/actor.hpp"
#include <utility>
#include "simple_platformer/actor/actor_validation.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/path_follower.hpp"

namespace simple_platformer
{
    namespace
    {
        AnimationSet animationsFor(const std::string& name)
        {
            if (name == "player")
            {
                return makePlayerAnimations();
            }
            if (name == "zombie")
            {
                return makeZombieAnimations();
            }
            if (name == "bat")
            {
                return makeBatAnimations();
            }
            if (name == "zombie_soldier")
            {
                return makeZombieSoldierAnimations();
            }
            throw std::invalid_argument("unknown animation set '" + name + "'");
        }
    }

    Actor composeActor(
        const ActorDefinition& definition,
        int textureId,
        glm::vec2 spawnFeet,
        std::optional<Patrol> patrol)
    {
        Actor actor;
        actor.body.bounds.size = definition.bodySize;
        placeFeetAt(actor.body.bounds, spawnFeet);
        actor.team = definition.team;
        actor.facing = definition.facing;
        if (definition.platformer)
        {
            const auto& config = *definition.platformer;
            validatePlatformerMovementConfig(config);
            actor.platformerMovement = PlatformerMovement{};
            actor.platformerMovement->config = config;
            actor.platformerMovement->grounded = true;
        }
        actor.flyingMovement = definition.flying;
        if (definition.health)
        {
            actor.health = Health{*definition.health, *definition.health};
        }
        if (definition.inventorySlots)
        {
            if (*definition.inventorySlots <= 0)
            {
                throw std::invalid_argument("inventorySlots must be positive");
            }
            actor.inventory = Inventory{static_cast<std::size_t>(*definition.inventorySlots)};
        }
        if (definition.senses)
        {
            actor.brain = NpcBrain{};
            actor.senses = definition.senses;
            actor.pathFollower = PathFollower{};
        }
        actor.patrol = patrol;
        actor.bite = definition.bite;
        if (actor.bite)
        {
            actor.bite->phase = BitePhase::Ready;
            actor.bite->phaseTimeRemaining = 0;
            actor.bite->actorsHit.clear();
        }
        actor.rangedWeapon = definition.ranged;
        if (actor.rangedWeapon)
        {
            actor.rangedWeapon->phase = RangedPhase::Ready;
            actor.rangedWeapon->phaseTimeRemaining = 0;
            actor.rangedWeapon->firedThisUpdate = false;
            actor.rangedWeapon->projectileSprite.textureId = textureId;
        }
        if (!definition.animations.empty())
        {
            Animator animator;
            animator.animationSet = animationsFor(definition.animations);
            const auto& frame = clipFor(animator.animationSet, AnimationName::Idle).frames.front();
            actor.sprite = Sprite{textureId, frame, frame.size};
            actor.sprite->anchor = definition.spriteAnchor;
            actor.animator = std::move(animator);
        }
        validateActor(actor);
        return actor;
    }

    void validateActorDefinition(const ActorDefinition& definition)
    {
        // Use the same composition and engine validation for loaded and C++ definitions.
        composeActor(definition, 0);
    }
}
