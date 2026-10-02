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
#include "simple_platformer/navigation/platformer_connection_table.hpp"
#include "simple_platformer/world/level_exit.hpp"
#include "simple_platformer/world/pickup.hpp"

namespace simple_platformer
{
    enum class NoiseKind
    {
        Landing,
        Shot
    };

    struct NoiseEvent
    {
        ActorId source;
        // Captured when emitted, not looked up from the actor when heard.
        glm::vec2 feet = {0.0F, 0.0F};
        NoiseKind kind = NoiseKind::Landing;
    };

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
        // Transfers what fits into the player's inventory and erases an emptied pickup.
        // Call after pickup iteration, because erasing shifts the remaining indexes.
        void collectPickup(std::size_t index);
        void setExit(LevelExit exit);
        const std::optional<LevelExit>& exit() const;
        std::optional<LevelExit>& exit();
        bool levelComplete() const;
        void completeLevel();

        // Elapsed simulation seconds. The clock and its stamps use double to preserve
        // precision in long sessions.
        double simulationTimeSeconds() const;
        // Returns a stamp's age as float seconds, or nothing if unset. The stamp must be
        // finite and between zero and this world's current time.
        std::optional<float> secondsSince(const std::optional<double>& timeSeconds) const;
        // The simulation loop calls this once at the start of each active update.
        void advanceSimulationTime(float deltaTime);
        // Events emitted after sensing are delivered on the next fixed update.
        // Sensing takes the batch once and shares it with every NPC before discarding it.
        void emitNoise(NoiseEvent event);
        std::vector<NoiseEvent> takeNoises();

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

        void addProjectileBurst(ProjectileBurst burst);
        bool removeProjectileBurst(std::size_t index);
        std::vector<ProjectileBurst>& projectileBursts();
        const std::vector<ProjectileBurst>& projectileBursts() const;

        void setPlayer(ActorId id, glm::vec2 spawnFeet);
        ActorId playerId() const;
        glm::vec2 playerSpawnFeet() const;
        void respawnPlayer();

        // Connections for this world's map, shared by actors with the same profile.
        // New profiles build once; tile breaks rebuild only affected cells.
        PlatformerConnectionTable& platformerConnections();
        const PlatformerConnectionTable& platformerConnections() const;

    private:
        void requireWithinSimulationTime(const std::optional<double>& time, const char* what) const;

        std::vector<ItemDefinition> itemDefinitions;
        PlatformerConnectionTable platformerConnectionTable;
        std::vector<Pickup> pickupStorage;
        std::optional<LevelExit> levelExit;
        bool completed = false;
        double elapsedSimulationTimeSeconds = 0.0;
        std::vector<Actor> actorStorage;
        std::vector<NoiseEvent> pendingNoises;
        std::vector<Projectile> projectileStorage;
        std::vector<ProjectileBurst> projectileBurstStorage;
        std::uint32_t nextActorId = 1;
        ActorId controlledPlayer;
        glm::vec2 controlledPlayerSpawnFeet = {0.0F, 0.0F};
    };
}
