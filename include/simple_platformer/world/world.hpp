#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/world/level_exit.hpp"
#include "simple_platformer/world/pickup.hpp"

namespace simple_platformer
{
    class World
    {
    public:
        // Definitions are immutable for the lifetime of a world; IDs may be shared across levels.
        explicit World(std::vector<ItemDefinition> items = {});
        const ItemDefinition& itemDefinition(ItemId id) const;
        void addPickup(Pickup pickup);
        // Collection can erase pickups. Do not retain references/indexes across request batches.
        std::vector<Pickup>& pickups();
        const std::vector<Pickup>& pickups() const;
        // Used when applying queued pickup requests, after iteration has finished.
        void collectPickup(std::size_t index);
        void setExit(LevelExit exit);
        const std::optional<LevelExit>& exit() const;
        bool levelComplete() const;
        void completeLevel();

        // Elapsed active fixed-step time for this world.
        float simulationTimeSeconds() const;
        // The simulation loop calls this once at the start of each active update.
        void advanceSimulationTime(float deltaTime);

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
        std::vector<ItemDefinition> itemDefinitions;
        std::vector<Pickup> pickupStorage;
        std::optional<LevelExit> levelExit;
        bool completed = false;
        float elapsedSimulationTimeSeconds = 0.0F;
        std::vector<Actor> actorStorage;
        std::vector<Projectile> projectileStorage;
        std::uint32_t nextActorId = 1;
        ActorId controlledPlayer;
        glm::vec2 controlledPlayerSpawnFeet = {0.0F, 0.0F};
    };
}
