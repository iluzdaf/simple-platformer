#include "simple_platformer/world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"

namespace
{
    bool isFinite(glm::vec2 value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y);
    }

    void validateActor(const simple_platformer::Actor& actor)
    {
        if (simple_platformer::isValid(actor.id))
        {
            throw std::invalid_argument("World assigns actor IDs");
        }
        if (!isFinite(actor.body.bounds.position) || !isFinite(actor.body.bounds.size) ||
            !isFinite(actor.body.velocity) || actor.body.bounds.size.x <= 0.0F ||
            actor.body.bounds.size.y <= 0.0F)
        {
            throw std::invalid_argument("Actors require finite positive-sized bodies");
        }
        if (!actor.platformerMovement.has_value())
        {
            throw std::invalid_argument("Phase 5 actors require platformer movement");
        }
        if (actor.animator.has_value() && !actor.sprite.has_value())
        {
            throw std::invalid_argument("Animated actors require a sprite");
        }
        if (actor.animator.has_value() &&
            (!std::isfinite(actor.animator->elapsed) || actor.animator->elapsed < 0.0F))
        {
            throw std::invalid_argument("Actor animation time must be finite and non-negative");
        }
        if (actor.health.has_value() && (actor.health->maximum <= 0 || actor.health->current < 0 ||
                                         actor.health->current > actor.health->maximum))
        {
            throw std::invalid_argument("Actor health must be within zero and its maximum");
        }
    }
}

namespace simple_platformer
{
    ActorId World::addActor(Actor actor)
    {
        validateActor(actor);
        if (nextActorId == std::numeric_limits<std::uint32_t>::max())
        {
            throw std::overflow_error("Actor IDs have been exhausted");
        }

        actor.id = {nextActorId};
        ++nextActorId;
        const ActorId id = actor.id;
        actorStorage.push_back(actor);
        return id;
    }

    bool World::removeActor(ActorId id)
    {
        const auto actor = std::find_if(
            actorStorage.begin(),
            actorStorage.end(),
            [id](const Actor& candidate) { return candidate.id == id; });
        if (actor == actorStorage.end())
        {
            return false;
        }

        actorStorage.erase(actor);
        if (controlledPlayer == id)
        {
            controlledPlayer = {};
        }
        return true;
    }

    Actor* World::findActor(ActorId id)
    {
        const auto actor = std::find_if(
            actorStorage.begin(),
            actorStorage.end(),
            [id](const Actor& candidate) { return candidate.id == id; });
        return actor == actorStorage.end() ? nullptr : &*actor;
    }

    const Actor* World::findActor(ActorId id) const
    {
        const auto actor = std::find_if(
            actorStorage.begin(),
            actorStorage.end(),
            [id](const Actor& candidate) { return candidate.id == id; });
        return actor == actorStorage.end() ? nullptr : &*actor;
    }

    std::vector<Actor>& World::actors()
    {
        return actorStorage;
    }

    const std::vector<Actor>& World::actors() const
    {
        return actorStorage;
    }

    void World::setPlayer(ActorId id, glm::vec2 spawnFeet)
    {
        if (findActor(id) == nullptr || !isFinite(spawnFeet))
        {
            throw std::invalid_argument("The player must be an existing actor with a finite spawn");
        }

        controlledPlayer = id;
        controlledPlayerSpawnFeet = spawnFeet;
    }

    ActorId World::playerId() const
    {
        return controlledPlayer;
    }

    glm::vec2 World::playerSpawnFeet() const
    {
        if (!isValid(controlledPlayer))
        {
            throw std::logic_error("The world has no player");
        }
        return controlledPlayerSpawnFeet;
    }

    void World::respawnPlayer()
    {
        Actor* player = findActor(controlledPlayer);
        if (player == nullptr)
        {
            throw std::logic_error("The world has no player to respawn");
        }

        placeFeetAt(player->body.bounds, controlledPlayerSpawnFeet);
        player->body.velocity = {0.0F, 0.0F};
        player->intentions = {};
        player->life = LifeState::Alive;
        player->deathTimeRemaining = 0.0F;
        if (player->health.has_value())
        {
            player->health->current = player->health->maximum;
        }
        if (player->platformerMovement.has_value())
        {
            player->platformerMovement->grounded = false;
            player->platformerMovement->coyoteRemaining = 0.0F;
            player->platformerMovement->jumpBufferRemaining = 0.0F;
        }
    }
}
