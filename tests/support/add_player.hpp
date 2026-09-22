#pragma once

#include <utility>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/world/world.hpp"

namespace tests
{
    // Adds the actor as the world's player, respawning where it stands.
    inline simple_platformer::ActorId addPlayer(
        simple_platformer::World& world,
        simple_platformer::Actor actor)
    {
        const glm::vec2 feet = simple_platformer::feetOf(actor.body.bounds);
        const simple_platformer::ActorId id = world.addActor(std::move(actor));
        world.setPlayer(id, feet);
        return id;
    }
}
