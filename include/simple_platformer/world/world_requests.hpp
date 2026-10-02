#pragma once

#include <cstddef>
#include <vector>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"

namespace simple_platformer
{
    class World;

    constexpr float ActorDeathSeconds = 0.4F;

    class WorldRequests
    {
    public:
        void damage(ActorId target, int amount);
        void remove(ActorId target);
        void spawnProjectile(Projectile projectile);
        void removeProjectile(std::size_t index);
        void spawnProjectileBurst(ProjectileBurst burst);
        void removeProjectileBurst(std::size_t index);
        void collectPickup(std::size_t index);
        void useItem(ActorId actor, std::size_t slot);
        bool empty() const;
        // Cleanup systems inspect this before the requests remove the actors from World.
        const std::vector<ActorId>& actorsToRemove() const;

    private:
        struct DamageRequest
        {
            ActorId target;
            int amount = 0;
        };

        struct UseItemRequest
        {
            ActorId actor;
            std::size_t slot = 0;
        };

        void applyDamage(World& world);
        void advanceLifeState(World& world, float deltaTime);

        friend void applyWorldRequests(World&, WorldRequests&, float);
        friend void applyWorldRequests(World&, WorldRequests&);

        std::vector<DamageRequest> damageRequests;
        std::vector<ActorId> removalRequests;
        std::vector<Projectile> projectileSpawns;
        std::vector<std::size_t> projectileRemovals;
        std::vector<ProjectileBurst> projectileBurstSpawns;
        std::vector<std::size_t> projectileBurstRemovals;
        std::vector<std::size_t> pickupCollections;
        std::vector<UseItemRequest> itemUses;
    };

    // Applies damage, item use, collection and structural changes without advancing time.
    // Use this for UI requests while paused.
    void applyWorldRequests(World& world, WorldRequests& requests);

    // Ends a simulation step: applies damage, advances existing death timers, detects
    // pickups after any respawn, then applies queued changes. New deaths keep their full
    // timer. Expired deaths remove NPCs or respawn the player.
    void applyWorldRequests(World& world, WorldRequests& requests, float deltaTime);
}
