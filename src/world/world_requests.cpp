#include "simple_platformer/world/world_requests.hpp"

#include <cstddef>
#include <stdexcept>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"

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

    bool WorldRequests::empty() const
    {
        return damageRequests.empty() && removalRequests.empty() && projectileSpawns.empty() &&
               projectileRemovals.empty();
    }
}
