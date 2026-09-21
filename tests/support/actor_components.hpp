#pragma once

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/world/world.hpp"

namespace tests
{
    // Reads an actor, or one of its components, back out of a World. Each fails the test
    // when the actor or component is missing, rather than letting it dereference nothing.

    inline simple_platformer::Actor& actor(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        simple_platformer::Actor* result = world.findActor(id);
        REQUIRE(result != nullptr);
        return *result;
    }

    inline simple_platformer::NpcBrain& brain(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        std::optional<simple_platformer::NpcBrain>& component = actor(world, id).brain;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no NPC brain");
        }
        return *component;
    }

    inline simple_platformer::BiteAttack& bite(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        std::optional<simple_platformer::BiteAttack>& component = actor(world, id).bite;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no bite");
        }
        return *component;
    }

    inline simple_platformer::RangedWeapon& rangedWeapon(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        std::optional<simple_platformer::RangedWeapon>& component = actor(world, id).rangedWeapon;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no ranged weapon");
        }
        return *component;
    }

    inline simple_platformer::PathFollower& pathFollower(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        std::optional<simple_platformer::PathFollower>& component = actor(world, id).pathFollower;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no path follower");
        }
        return *component;
    }

    inline simple_platformer::Patrol& patrol(
        simple_platformer::World& world,
        simple_platformer::ActorId id)
    {
        std::optional<simple_platformer::Patrol>& component = actor(world, id).patrol;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no patrol");
        }
        return *component;
    }
}
