#include "simple_platformer/actor/lifecycle.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"

namespace simple_platformer
{
    void updateLifeState(
        World& world,
        WorldRequests& requests,
        float deltaTime,
        float deathDuration)
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0F || !std::isfinite(deathDuration) ||
            deathDuration <= 0.0F)
        {
            throw std::invalid_argument("Lifecycle timing must be finite and positive");
        }

        std::vector<ActorId> actorsAlreadyDying;
        for (const Actor& actor : world.actors())
        {
            if (actor.life == LifeState::Dying)
            {
                actorsAlreadyDying.push_back(actor.id);
            }
        }

        for (const auto& request : requests.damageRequests)
        {
            Actor* actor = world.findActor(request.target);
            if (actor == nullptr || actor->life != LifeState::Alive || !actor->health.has_value())
            {
                continue;
            }

            actor->health->current = std::max(0, actor->health->current - request.amount);
            if (actor->health->current == 0)
            {
                actor->life = LifeState::Dying;
                actor->deathTimeRemaining = deathDuration;
                actor->intentions = {};
            }
        }

        for (const ActorId id : actorsAlreadyDying)
        {
            Actor* actor = world.findActor(id);
            if (actor == nullptr)
            {
                continue;
            }

            actor->deathTimeRemaining = std::max(0.0F, actor->deathTimeRemaining - deltaTime);
            if (actor->deathTimeRemaining > 0.0F)
            {
                continue;
            }

            if (id == world.playerId())
            {
                world.respawnPlayer();
            }
            else
            {
                requests.removalRequests.push_back(id);
            }
        }

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
            if (*removal < world.projectiles().size())
            {
                world.projectiles().erase(
                    world.projectiles().begin() + static_cast<std::ptrdiff_t>(*removal));
            }
        }

        for (const Projectile& projectile : requests.projectileSpawns)
        {
            world.addProjectile(projectile);
        }

        requests.damageRequests.clear();
        requests.removalRequests.clear();
        requests.projectileSpawns.clear();
        requests.projectileRemovals.clear();
    }
}
