#include "simple_platformer/world/world.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_validation.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/path_follower.hpp"

namespace simple_platformer
{
    namespace
    {
        using simple_platformer::isFinite;

        bool isFinitePositive(float value)
        {
            return std::isfinite(value) && value > 0.0F;
        }

        void validateProjectile(const simple_platformer::Projectile& projectile)
        {
            if (!isFinite(projectile.bounds.position) || !isFinite(projectile.bounds.size) ||
                projectile.bounds.size.x <= 0.0F || projectile.bounds.size.y <= 0.0F ||
                !isFinite(projectile.velocity) || projectile.damage <= 0 ||
                !isFinitePositive(projectile.remainingLifetime) ||
                !isFinite(projectile.sprite.size) || projectile.sprite.size.x <= 0.0F ||
                projectile.sprite.size.y <= 0.0F ||
                (projectile.owner.has_value() && !simple_platformer::isValid(*projectile.owner)))
            {
                throw std::invalid_argument("Projectile data is invalid");
            }
        }

        void validateProjectileBurst(const simple_platformer::ProjectileBurst& burst)
        {
            const bool hasValidCause =
                burst.cause == simple_platformer::ProjectileBurstCause::Impact ||
                burst.cause == simple_platformer::ProjectileBurstCause::LifetimeExpired;
            if (!hasValidCause || !isFinite(burst.center) || !isFinite(burst.direction) ||
                (burst.direction.x == 0.0F && burst.direction.y == 0.0F) ||
                !isFinite(burst.sprite.size) || burst.sprite.size.x <= 0.0F ||
                burst.sprite.size.y <= 0.0F || !isFinitePositive(burst.duration) ||
                !isFinitePositive(burst.remainingLifetime) ||
                burst.remainingLifetime > burst.duration)
            {
                throw std::invalid_argument("Projectile burst data is invalid");
            }
        }

    }

    World::World(std::vector<ItemDefinition> items) : itemDefinitions(std::move(items))
    {
        for (std::size_t index = 0; index < itemDefinitions.size(); ++index)
        {
            validateItemDefinition(itemDefinitions[index]);
            for (std::size_t previous = 0; previous < index; ++previous)
            {
                if (itemDefinitions[previous].id == itemDefinitions[index].id)
                {
                    throw std::invalid_argument("Item IDs must be unique");
                }
            }
        }
    }

    float World::simulationTimeSeconds() const
    {
        return elapsedSimulationTimeSeconds;
    }

    void World::advanceSimulationTime(float deltaTime)
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0F)
        {
            throw std::invalid_argument("Simulation delta time must be finite and non-negative");
        }
        const float advancedTime = elapsedSimulationTimeSeconds + deltaTime;
        if (!std::isfinite(advancedTime))
        {
            throw std::overflow_error("Simulation time has overflowed");
        }
        elapsedSimulationTimeSeconds = advancedTime;
    }

    const ItemDefinition& World::itemDefinition(ItemId id) const
    {
        for (const ItemDefinition& definition : itemDefinitions)
        {
            if (definition.id == id)
            {
                return definition;
            }
        }
        throw std::invalid_argument("Unknown item ID");
    }

    ActorId World::addActor(Actor actor)
    {
        validateActor(actor);
        if (actor.lastDamageTimeSeconds.has_value() &&
            (!std::isfinite(actor.lastDamageTimeSeconds.value()) ||
             actor.lastDamageTimeSeconds.value() < 0.0F ||
             actor.lastDamageTimeSeconds.value() > elapsedSimulationTimeSeconds))
        {
            throw std::invalid_argument("Actor damage time must be within simulation time");
        }
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

    void World::addProjectile(Projectile projectile)
    {
        validateProjectile(projectile);
        projectileStorage.push_back(projectile);
    }

    bool World::removeProjectile(std::size_t index)
    {
        if (index >= projectileStorage.size())
        {
            return false;
        }

        projectileStorage.erase(projectileStorage.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }

    std::vector<Projectile>& World::projectiles()
    {
        return projectileStorage;
    }

    const std::vector<Projectile>& World::projectiles() const
    {
        return projectileStorage;
    }

    void World::addProjectileBurst(ProjectileBurst burst)
    {
        validateProjectileBurst(burst);
        projectileBurstStorage.push_back(burst);
    }

    bool World::removeProjectileBurst(std::size_t index)
    {
        if (index >= projectileBurstStorage.size())
        {
            return false;
        }

        projectileBurstStorage.erase(
            projectileBurstStorage.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }

    std::vector<ProjectileBurst>& World::projectileBursts()
    {
        return projectileBurstStorage;
    }

    const std::vector<ProjectileBurst>& World::projectileBursts() const
    {
        return projectileBurstStorage;
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
        player->lastDamageTimeSeconds.reset();
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
