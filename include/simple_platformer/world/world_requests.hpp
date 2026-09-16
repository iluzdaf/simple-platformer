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
        bool empty() const;

    private:
        struct DamageRequest
        {
            ActorId target;
            int amount = 0;
        };

        friend void updateLifeState(World&, WorldRequests&, float, float);
        friend void applyWorldRequests(World&, WorldRequests&);

        std::vector<DamageRequest> damageRequests;
        std::vector<ActorId> removalRequests;
        std::vector<Projectile> projectileSpawns;
        std::vector<std::size_t> projectileRemovals;
    };

    // Applies queued structural changes after systems have finished traversing the World.
    void applyWorldRequests(World& world, WorldRequests& requests);
}
