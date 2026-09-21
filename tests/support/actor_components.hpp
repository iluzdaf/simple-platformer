#pragma once

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/inventory/inventory.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/world/world.hpp"

namespace tests
{
    // Reads an actor back out of a World, or one of its components out of an actor or a World.
    // Each fails the test when the actor or component is missing, rather than letting it
    // dereference nothing.

    inline simple_platformer::Actor& actor(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        simple_platformer::Actor* result = world.findActor(id);
        REQUIRE(result != nullptr);
        return *result;
    }

    // The actor the world treats as its player.
    inline simple_platformer::Actor& player(simple_platformer::World& world)
    {
        return actor(world, world.playerId());
    }

    inline simple_platformer::Health& health(simple_platformer::Actor& actor)
    {
        std::optional<simple_platformer::Health>& component = actor.health;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no health");
        }
        return *component;
    }

    inline simple_platformer::Health& health(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        return health(actor(world, id));
    }

    inline simple_platformer::Inventory& inventory(simple_platformer::Actor& actor)
    {
        std::optional<simple_platformer::Inventory>& component = actor.inventory;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no inventory");
        }
        return *component;
    }

    inline simple_platformer::Inventory& inventory(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        return inventory(actor(world, id));
    }

    inline simple_platformer::Animator& animator(simple_platformer::Actor& actor)
    {
        std::optional<simple_platformer::Animator>& component = actor.animator;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no animator");
        }
        return *component;
    }

    inline simple_platformer::Animator& animator(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        return animator(actor(world, id));
    }

    inline simple_platformer::NpcBrain& brain(simple_platformer::Actor& actor)
    {
        std::optional<simple_platformer::NpcBrain>& component = actor.brain;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no NPC brain");
        }
        return *component;
    }

    inline simple_platformer::NpcBrain& brain(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        return brain(actor(world, id));
    }

    inline simple_platformer::BiteAttack& bite(simple_platformer::Actor& actor)
    {
        std::optional<simple_platformer::BiteAttack>& component = actor.bite;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no bite");
        }
        return *component;
    }

    inline simple_platformer::BiteAttack& bite(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        return bite(actor(world, id));
    }

    inline simple_platformer::RangedWeapon& rangedWeapon(simple_platformer::Actor& actor)
    {
        std::optional<simple_platformer::RangedWeapon>& component = actor.rangedWeapon;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no ranged weapon");
        }
        return *component;
    }

    inline simple_platformer::RangedWeapon& rangedWeapon(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        return rangedWeapon(actor(world, id));
    }

    inline simple_platformer::PathFollower& pathFollower(simple_platformer::Actor& actor)
    {
        std::optional<simple_platformer::PathFollower>& component = actor.pathFollower;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no path follower");
        }
        return *component;
    }

    inline simple_platformer::PathFollower& pathFollower(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        return pathFollower(actor(world, id));
    }

    inline simple_platformer::Patrol& patrol(simple_platformer::Actor& actor)
    {
        std::optional<simple_platformer::Patrol>& component = actor.patrol;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no patrol");
        }
        return *component;
    }

    inline simple_platformer::Patrol& patrol(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        return patrol(actor(world, id));
    }

    inline simple_platformer::PlatformerMovement& platformerMovement(
        simple_platformer::Actor& actor)
    {
        std::optional<simple_platformer::PlatformerMovement>& component = actor.platformerMovement;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no platformer movement");
        }
        return *component;
    }

    inline simple_platformer::PlatformerMovement& platformerMovement(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        return platformerMovement(actor(world, id));
    }
}
