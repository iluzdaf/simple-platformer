#include "simple_platformer/actor/lifecycle.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "simple_platformer/actor/actor.hpp"
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

    bool WorldRequests::empty() const
    {
        return damageRequests.empty() && removalRequests.empty();
    }

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

        for (const DamageRequest& request : requests.damageRequests)
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

        requests.damageRequests.clear();
        requests.removalRequests.clear();
    }
}
