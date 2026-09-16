#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"

namespace simple_platformer
{
    class World
    {
    public:
        ActorId addActor(Actor actor);
        bool removeActor(ActorId id);

        // Adding or removing actors invalidates returned pointers. Keep ActorId values across
        // world mutations and use these pointers only for temporary access.
        Actor* findActor(ActorId id);
        const Actor* findActor(ActorId id) const;

        // Systems may modify existing actors through this collection. Adding or removing actors
        // must go through addActor() and removeActor() so World can preserve its invariants.
        std::vector<Actor>& actors();
        const std::vector<Actor>& actors() const;

        void addProjectile(Projectile projectile);
        bool removeProjectile(std::size_t index);

        // Systems may modify existing projectiles through this collection. Adding or removing
        // projectiles must go through World so projectile data remains valid.
        std::vector<Projectile>& projectiles();
        const std::vector<Projectile>& projectiles() const;

        void setPlayer(ActorId id, glm::vec2 spawnFeet);
        ActorId playerId() const;
        glm::vec2 playerSpawnFeet() const;
        void respawnPlayer();

    private:
        std::vector<Actor> actorStorage;
        std::vector<Projectile> projectileStorage;
        std::uint32_t nextActorId = 1;
        ActorId controlledPlayer;
        glm::vec2 controlledPlayerSpawnFeet = {0.0F, 0.0F};
    };
}
