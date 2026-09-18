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
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"

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

        void validateIdentity(const simple_platformer::Actor& actor)
        {
            if (simple_platformer::isValid(actor.id))
            {
                throw std::invalid_argument("World assigns actor IDs");
            }
        }

        void validateBody(const simple_platformer::Actor& actor)
        {
            if (!isFinite(actor.body.bounds.position) || !isFinite(actor.body.bounds.size) ||
                !isFinite(actor.body.velocity) || actor.body.bounds.size.x <= 0.0F ||
                actor.body.bounds.size.y <= 0.0F)
            {
                throw std::invalid_argument("Actors require finite positive-sized bodies");
            }
        }

        void validateMovement(const simple_platformer::Actor& actor)
        {
            if (actor.platformerMovement.has_value() == actor.flyingMovement.has_value())
            {
                throw std::invalid_argument("Actors require exactly one movement component");
            }
            if (actor.flyingMovement.has_value() &&
                (!std::isfinite(actor.flyingMovement->speed) || actor.flyingMovement->speed < 0.0F))
            {
                throw std::invalid_argument(
                    "Flying movement speed must be finite and non-negative");
            }
        }

        void validatePresentation(const simple_platformer::Actor& actor)
        {
            if (actor.animator.has_value() && !actor.sprite.has_value())
            {
                throw std::invalid_argument("Animated actors require a sprite");
            }
            if (actor.animator.has_value() &&
                (!std::isfinite(actor.animator->elapsed) || actor.animator->elapsed < 0.0F ||
                 actor.animator->animationSet.clips.empty()))
            {
                throw std::invalid_argument("Actor animation data is invalid");
            }
        }

        void validateCombat(const simple_platformer::Actor& actor)
        {
            if (actor.rangedWeapon.has_value())
            {
                const simple_platformer::RangedWeapon& weapon = *actor.rangedWeapon;
                if (weapon.damage <= 0 || !isFinite(weapon.projectileSize) ||
                    weapon.projectileSize.x <= 0.0F || weapon.projectileSize.y <= 0.0F ||
                    !isFinitePositive(weapon.projectileSpeed) ||
                    !isFinitePositive(weapon.projectileLifetime) ||
                    !isFinitePositive(weapon.shootDuration) ||
                    !isFinitePositive(weapon.recoveryDuration) ||
                    !std::isfinite(weapon.phaseTimeRemaining) || weapon.phaseTimeRemaining < 0.0F ||
                    !isFinite(weapon.projectileSprite.size) ||
                    weapon.projectileSprite.size.x <= 0.0F ||
                    weapon.projectileSprite.size.y <= 0.0F)
                {
                    throw std::invalid_argument("Actor ranged weapon data is invalid");
                }
            }
            if (actor.bite.has_value())
            {
                const simple_platformer::BiteAttack& bite = *actor.bite;
                if (bite.damage <= 0 || !isFinite(bite.hitboxSize) || bite.hitboxSize.x <= 0.0F ||
                    bite.hitboxSize.y <= 0.0F || !std::isfinite(bite.reach) || bite.reach < 0.0F ||
                    !isFinitePositive(bite.windupDuration) ||
                    !isFinitePositive(bite.activeDuration) ||
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
            if (actor.health.has_value() &&
                (actor.health->maximum <= 0 || actor.health->current < 0 ||
                 actor.health->current > actor.health->maximum))
            {
                throw std::invalid_argument("Actor health must be within zero and its maximum");
            }
        }

        void validateNpc(const simple_platformer::Actor& actor)
        {
            const bool hasAnyNpcComponent = actor.brain.has_value() || actor.senses.has_value() ||
                                            actor.patrol.has_value() ||
                                            actor.pathFollower.has_value();
            const bool hasRequiredNpcComponents = actor.brain.has_value() &&
                                                  actor.senses.has_value() &&
                                                  actor.pathFollower.has_value();
            if (hasAnyNpcComponent && !hasRequiredNpcComponents)
            {
                throw std::invalid_argument(
                    "NPC actors require a brain, senses, and path follower");
            }
            if (actor.brain.has_value() &&
                actor.brain->state == simple_platformer::NpcState::Bite && !actor.bite.has_value())
            {
                throw std::invalid_argument("An NPC in the Bite state requires a bite attack");
            }
            if (actor.brain.has_value() &&
                (!std::isfinite(actor.brain->stateTime) || actor.brain->stateTime < 0.0F ||
                 !isFinite(actor.brain->lastSeenTargetFeet) ||
                 !std::isfinite(actor.brain->targetMemoryRemaining) ||
                 actor.brain->targetMemoryRemaining < 0.0F))
            {
                throw std::invalid_argument("NPC brain runtime data is invalid");
            }
            if (actor.senses.has_value() &&
                (!std::isfinite(actor.senses->noticeDistance) ||
                 actor.senses->noticeDistance < 0.0F || !std::isfinite(actor.senses->forgetAfter) ||
                 actor.senses->forgetAfter < 0.0F))
            {
                throw std::invalid_argument("NPC senses data is invalid");
            }
            if (actor.patrol.has_value() &&
                (!isFinite(actor.patrol->firstFeet) || !isFinite(actor.patrol->secondFeet)))
            {
                throw std::invalid_argument("NPC patrol endpoints must be finite");
            }
        }

        void validatePathFollower(const simple_platformer::Actor& actor)
        {
            if (actor.pathFollower.has_value() &&
                (!std::isfinite(actor.pathFollower->repathCooldown) ||
                 actor.pathFollower->repathCooldown <= 0.0F ||
                 !std::isfinite(actor.pathFollower->repathRemaining) ||
                 actor.pathFollower->repathRemaining < 0.0F ||
                 !std::isfinite(actor.pathFollower->programElapsed) ||
                 actor.pathFollower->programElapsed < 0.0F))
            {
                throw std::invalid_argument("NPC path timing is invalid");
            }
        }

        void validateActor(const simple_platformer::Actor& actor)
        {
            validateIdentity(actor);
            validateBody(actor);
            validateMovement(actor);
            validatePresentation(actor);
            validateCombat(actor);
            validateNpc(actor);
            validatePathFollower(actor);
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
