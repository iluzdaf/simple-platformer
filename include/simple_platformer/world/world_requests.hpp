#pragma once

#include <cstddef>
#include <vector>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"

namespace simple_platformer
{
    class World;

    class WorldRequests
    {
    public:
        void damage(ActorId target, int amount);
        void remove(ActorId target);
        void spawnProjectile(Projectile projectile);
        void removeProjectile(std::size_t index);
        void collectPickup(std::size_t index);
        void useItem(ActorId actor, std::size_t slot);
        bool empty() const;

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

        friend void updateLifeState(World&, WorldRequests&, float, float);
        friend void applyWorldRequests(World&, WorldRequests&);

        std::vector<DamageRequest> damageRequests;
        std::vector<ActorId> removalRequests;
        std::vector<Projectile> projectileSpawns;
        std::vector<std::size_t> projectileRemovals;
        std::vector<std::size_t> pickupCollections;
        std::vector<UseItemRequest> itemUses;
    };

    // Applies item use, collection and structural changes after systems finish traversing World.
    void applyWorldRequests(World& world, WorldRequests& requests);
}
