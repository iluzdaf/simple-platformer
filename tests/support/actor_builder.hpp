#pragma once

#include <utility>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/inventory/inventory.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/platformer_cells.hpp"
#include "simple_platformer/navigation/route.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_state_machine.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "support/tile_size.hpp"

namespace tests
{
    // Builds an actor for a test, from the geometry and components the test chooses
    // rather than from shipped content:
    //
    //   ActorBuilder::sized({12.0F, 12.0F})
    //       .inCell({3, 1})
    //       .platforming()
    //       .climbing({60.0F})
    //       .thinking({64.0F, 1.0F})
    //       .running(machine)
    //
    // The build rules, which the chain enforces at compile time:
    //
    // 1. sized() comes first, and offers only a placement: at(), atFeet(), inCell() or
    //    restingAt().
    // 2. A placement offers only a movement: platforming() or flying(). Every actor gets
    //    exactly one, which World requires.
    // 3. After the movement, the optional components may come in any order.
    // 4. thinking() makes the actor an NPC, adding its brain, perception, senses and path
    //    follower together. Only then can running() give it a state machine, which is
    //    started and validated as it is added.
    //
    // The chain converts to an Actor wherever one is expected. It checks nothing beyond
    // its order: World validates the result when the actor is added, so climbing() on a
    // flyer builds, and World rejects it.
    class ActorBuilder
    {
    public:
        class Sized;
        class Placed;
        class Thinking;

        static Sized sized(glm::vec2 size);

        ActorBuilder onTeam(simple_platformer::Team team) &&
        {
            built.team = team;
            return std::move(*this);
        }

        Thinking thinking(simple_platformer::NpcSenses senses) &&;

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

        // Only a platforming actor can climb. World rejects a climbing flyer.
        ActorBuilder climbing(simple_platformer::SurfaceClimbConfig config = {}) &&
        {
            built.surfaceClimb = simple_platformer::SurfaceClimb{config};
            return std::move(*this);
        }

        ActorBuilder biting(simple_platformer::BiteAttack bite = {}) &&
        {
            built.bite = std::move(bite);
            return std::move(*this);
        }

        ActorBuilder withContactDamage(simple_platformer::ContactDamage contact = {}) &&
        {
            built.contactDamage = std::move(contact);
            return std::move(*this);
        }

        ActorBuilder shooting(simple_platformer::RangedWeapon weapon = {}) &&
        {
            built.rangedWeapon = weapon;
            return std::move(*this);
        }

        operator simple_platformer::Actor() &&
        {
            return std::move(built);
        }

    protected:
        explicit ActorBuilder(simple_platformer::Actor actor)
            : built(std::move(actor))
        {
        }

        simple_platformer::Actor built;
    };

    // An NPC: everything an ActorBuilder offers, and running().
    class ActorBuilder::Thinking : public ActorBuilder
    {
    public:
        Thinking running(simple_platformer::NpcStateMachine machine) &&
        {
            built.machine = simple_platformer::startNpcMachine(std::move(machine));
            return std::move(*this);
        }

    private:
        friend class ActorBuilder;

        explicit Thinking(simple_platformer::Actor actor)
            : ActorBuilder(std::move(actor))
        {
        }
    };

    inline ActorBuilder::Thinking ActorBuilder::thinking(simple_platformer::NpcSenses senses) &&
    {
        built.brain = simple_platformer::NpcBrain{};
        built.perception = simple_platformer::NpcPerception{};
        built.senses = senses;
        built.pathFollower = simple_platformer::PathFollower{};
        return Thinking(std::move(built));
    }

    // A sized, placed body waiting for its one movement component.
    class ActorBuilder::Placed
    {
    public:
        ActorBuilder platforming(simple_platformer::PlatformerMovementConfig config = {}) &&
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

    // A body size waiting for its placement.
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
            return Placed(simple_platformer::boxStandingOn(feet, size));
        }

        // Standing in the cell, its feet on the middle of the cell's bottom edge. Every test
        // map has tests::TileSize tiles, so the cell is unambiguous.
        Placed inCell(simple_platformer::Cell cell) &&
        {
            return Placed(simple_platformer::boxInCell(TileSize, cell, size));
        }

        // Resting at the location: standing on the cell's floor, flush against its wall, or
        // hanging from its ceiling, where a search would start from it.
        Placed restingAt(simple_platformer::RouteLocation location) &&
        {
            return Placed(simple_platformer::boundsAtSurface(TileSize, location, size));
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
