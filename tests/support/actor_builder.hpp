#pragma once

#include <utility>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/inventory/inventory.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/sprite.hpp"

namespace tests
{
    // Builds an actor that World accepts, from the components and settings a test asks for.
    // The builder knows how components fit together, such as movement being required and
    // an NPC needing a brain, senses, and path follower at once. It holds no gameplay values:
    // a test supplies every size, position, speed, and range it depends on, so a passing test
    // says which ones mattered. A real player or NPC comes from the game's composition recipe.
    //
    // A chain reads what the actor is, where it is, how it moves, then what else it does:
    //
    //   ActorBuilder::sized({12.0F, 20.0F}).atFeet({24.0F, 32.0F}).walking().thatBites()
    //
    // Placement and movement can't be skipped: sized() offers only at() and atFeet(), and
    // those offer only walking() and flying(). A forgotten placement would silently put the
    // actor at the origin, and World requires exactly one movement component. The chain works
    // on a fresh builder and converts to an Actor wherever one is expected, such as
    // World::addActor.
    class ActorBuilder
    {
    public:
        class Sized;
        class Placed;

        static Sized sized(glm::vec2 size);

        ActorBuilder onTeam(simple_platformer::Team team) &&
        {
            built.team = team;
            return std::move(*this);
        }

        ActorBuilder thinking(simple_platformer::NpcSenses senses) &&
        {
            built.brain = simple_platformer::NpcBrain{};
            built.senses = senses;
            built.pathFollower = simple_platformer::PathFollower{};
            return std::move(*this);
        }

        ActorBuilder patrolling(glm::vec2 firstFeet, glm::vec2 secondFeet) &&
        {
            built.patrol = simple_platformer::Patrol{firstFeet, secondFeet, true};
            return std::move(*this);
        }

        ActorBuilder withHealth(int current, int maximum) &&
        {
            built.health = simple_platformer::Health{current, maximum};
            return std::move(*this);
        }

        ActorBuilder withInventory(simple_platformer::Inventory inventory) &&
        {
            built.inventory = std::move(inventory);
            return std::move(*this);
        }

        ActorBuilder withSprite(simple_platformer::Sprite sprite) &&
        {
            built.sprite = sprite;
            return std::move(*this);
        }

        ActorBuilder withAnimator(simple_platformer::Animator animator) &&
        {
            built.animator = std::move(animator);
            return std::move(*this);
        }

        ActorBuilder thatBites(simple_platformer::BiteAttack bite = {}) &&
        {
            built.bite = std::move(bite);
            return std::move(*this);
        }

        ActorBuilder thatShoots(simple_platformer::RangedWeapon weapon = {}) &&
        {
            built.rangedWeapon = weapon;
            return std::move(*this);
        }

        operator simple_platformer::Actor() &&
        {
            return std::move(built);
        }

    private:
        explicit ActorBuilder(simple_platformer::Actor actor)
            : built(std::move(actor))
        {
        }

        simple_platformer::Actor built;
    };

    // A placed body, waiting for the movement World requires.
    class ActorBuilder::Placed
    {
    public:
        ActorBuilder walking(simple_platformer::PlatformerMovementConfig config = {}) &&
        {
            built.platformerMovement = simple_platformer::PlatformerMovement{config};
            return ActorBuilder(std::move(built));
        }

        ActorBuilder flying(float speed) &&
        {
            built.flyingMovement = simple_platformer::FlyingMovement{speed};
            return ActorBuilder(std::move(built));
        }

    private:
        friend class ActorBuilder::Sized;

        explicit Placed(const simple_platformer::Aabb& bounds)
        {
            built.body.bounds = bounds;
        }

        simple_platformer::Actor built;
    };

    // A body's size, waiting for where it is.
    class ActorBuilder::Sized
    {
    public:
        // By its top-left corner.
        Placed at(glm::vec2 topLeft) &&
        {
            return Placed({topLeft, size});
        }

        // By the middle of its bottom edge, where the game places actors.
        Placed atFeet(glm::vec2 feet) &&
        {
            simple_platformer::Aabb bounds{{0.0F, 0.0F}, size};
            simple_platformer::placeFeetAt(bounds, feet);
            return Placed(bounds);
        }

    private:
        friend class ActorBuilder;

        explicit Sized(glm::vec2 bodySize)
            : size(bodySize)
        {
        }

        glm::vec2 size;
    };

    inline ActorBuilder::Sized ActorBuilder::sized(glm::vec2 size)
    {
        return Sized(size);
    }
}
