#include "simple_platformer/world/world_requests.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/inventory/item_use.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    void WorldRequests::damage(ActorId target, int amount)
    {
        if (!isValid(target) || amount <= 0)
        {
            throw std::invalid_argument("Damage requests require a target and positive amount");
        }
        damageRequests.push_back({target, amount});
    }

    void WorldRequests::remove(ActorId target)
    {
        if (!isValid(target))
        {
            throw std::invalid_argument("Removal requests require a target");
        }
        removalRequests.push_back(target);
    }

    void WorldRequests::spawnProjectile(Projectile projectile)
    {
        projectileSpawns.push_back(projectile);
    }

    void WorldRequests::removeProjectile(std::size_t index)
    {
        projectileRemovals.push_back(index);
    }

    void WorldRequests::spawnProjectileBurst(ProjectileBurst burst)
    {
        projectileBurstSpawns.push_back(burst);
    }

    void WorldRequests::removeProjectileBurst(std::size_t index)
    {
        projectileBurstRemovals.push_back(index);
    }

    bool WorldRequests::empty() const
    {
        return damageRequests.empty() && removalRequests.empty() && projectileSpawns.empty() &&
               projectileRemovals.empty() && projectileBurstSpawns.empty() &&
               projectileBurstRemovals.empty() && pickupCollections.empty() && itemUses.empty();
    }

    void WorldRequests::collectPickup(std::size_t index)
    {
        pickupCollections.push_back(index);
    }

    void WorldRequests::useItem(ActorId actor, std::size_t slot)
    {
        if (!isValid(actor))
        {
            throw std::invalid_argument("Item use requires an actor");
        }
        itemUses.push_back({actor, slot});
    }

    void applyWorldRequests(World& world, WorldRequests& requests)
    {
        for (const auto& use : requests.itemUses)
        {
            useItem(world, use.actor, use.slot);
        }

        std::sort(requests.pickupCollections.begin(), requests.pickupCollections.end());
        requests.pickupCollections.erase(
            std::unique(requests.pickupCollections.begin(), requests.pickupCollections.end()),
            requests.pickupCollections.end());
        for (auto pickup = requests.pickupCollections.rbegin();
             pickup != requests.pickupCollections.rend();
             ++pickup)
        {
            world.collectPickup(*pickup);
        }
        requests.itemUses.clear();
        requests.pickupCollections.clear();

        for (const ActorId id : requests.removalRequests)
        {
            world.removeActor(id);
        }

        std::sort(requests.projectileRemovals.begin(), requests.projectileRemovals.end());
        requests.projectileRemovals.erase(
            std::unique(requests.projectileRemovals.begin(), requests.projectileRemovals.end()),
            requests.projectileRemovals.end());
        for (auto removal = requests.projectileRemovals.rbegin();
             removal != requests.projectileRemovals.rend();
             ++removal)
        {
            world.removeProjectile(*removal);
        }

        for (const Projectile& projectile : requests.projectileSpawns)
        {
            world.addProjectile(projectile);
        }

        std::sort(requests.projectileBurstRemovals.begin(), requests.projectileBurstRemovals.end());
        requests.projectileBurstRemovals.erase(
            std::unique(
                requests.projectileBurstRemovals.begin(), requests.projectileBurstRemovals.end()),
            requests.projectileBurstRemovals.end());
        for (auto removal = requests.projectileBurstRemovals.rbegin();
             removal != requests.projectileBurstRemovals.rend();
             ++removal)
        {
            world.removeProjectileBurst(*removal);
        }

        for (const ProjectileBurst& burst : requests.projectileBurstSpawns)
        {
            world.addProjectileBurst(burst);
        }

        requests.removalRequests.clear();
        requests.projectileSpawns.clear();
        requests.projectileRemovals.clear();
        requests.projectileBurstSpawns.clear();
        requests.projectileBurstRemovals.clear();
    }
}
