#include "simple_platformer/world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"

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
            !isFinitePositive(projectile.remainingLifetime) || !isFinite(projectile.sprite.size) ||
            projectile.sprite.size.x <= 0.0F || projectile.sprite.size.y <= 0.0F ||
            (projectile.owner.has_value() && !simple_platformer::isValid(*projectile.owner)))
        {
            throw std::invalid_argument("Projectile data is invalid");
        }
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
            throw std::invalid_argument("Actors currently require platformer movement");
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
        if (actor.rangedWeapon.has_value())
        {
            const simple_platformer::RangedWeapon& weapon = *actor.rangedWeapon;
            if (weapon.damage <= 0 || !isFinite(weapon.projectileSize) ||
                weapon.projectileSize.x <= 0.0F || weapon.projectileSize.y <= 0.0F ||
                !isFinitePositive(weapon.projectileSpeed) ||
                !isFinitePositive(weapon.projectileLifetime) ||
                !isFinitePositive(weapon.cooldown) || !std::isfinite(weapon.cooldownRemaining) ||
                weapon.cooldownRemaining < 0.0F || !isFinite(weapon.projectileSprite.size) ||
                weapon.projectileSprite.size.x <= 0.0F || weapon.projectileSprite.size.y <= 0.0F)
            {
                throw std::invalid_argument("Actor ranged weapon data is invalid");
            }
        }
        if (actor.bite.has_value())
        {
            const simple_platformer::BiteAttack& bite = *actor.bite;
            if (bite.damage <= 0 || !isFinite(bite.hitboxSize) || bite.hitboxSize.x <= 0.0F ||
                bite.hitboxSize.y <= 0.0F || !std::isfinite(bite.reach) || bite.reach < 0.0F ||
                !isFinitePositive(bite.windupDuration) || !isFinitePositive(bite.activeDuration) ||
                !isFinitePositive(bite.recoveryDuration) ||
                !std::isfinite(bite.phaseTimeRemaining) || bite.phaseTimeRemaining < 0.0F)
            {
                throw std::invalid_argument("Actor bite data is invalid");
            }
        }
        if (actor.rangedWeapon.has_value() && actor.bite.has_value())
        {
            throw std::invalid_argument("An actor can have only one primary attack");
        }
        if ((actor.rangedWeapon.has_value() || actor.bite.has_value()) &&
            actor.team == simple_platformer::Team::Neutral)
        {
            throw std::invalid_argument("Actors with attacks require a non-neutral team");
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

    void World::addProjectile(Projectile projectile)
    {
        validateProjectile(projectile);
        projectileStorage.push_back(projectile);
    }

    std::vector<Projectile>& World::projectiles()
    {
        return projectileStorage;
    }

    const std::vector<Projectile>& World::projectiles() const
    {
        return projectileStorage;
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
