#include "simple_platformer/actor/actor_lifecycle.hpp"

#include <algorithm>
#include <vector>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"

namespace simple_platformer
{
    void applyDamageRequests(World& world, WorldRequests& requests)
    {
        for (const auto& request : requests.damageRequests)
        {
            Actor* actor = world.findActor(request.target);
            if (actor == nullptr || actor->life != LifeState::Alive || !actor->health.has_value())
            {
                continue;
            }

            actor->health->current = std::max(0, actor->health->current - request.amount);
            actor->lastDamageTimeSeconds = world.simulationTimeSeconds();
            if (actor->health->current == 0)
            {
                actor->life = LifeState::Dying;
                actor->deathTimeRemaining = ActorDeathSeconds;
                actor->intentions = {};
            }
        }
        requests.damageRequests.clear();
    }

    void updateActorLifecycle(World& world, WorldRequests& requests, float deltaTime)
    {
        requireSeconds(deltaTime, "Actor lifecycle time step");
        // Snapshot before applying damage so a newly dying actor keeps its full timer.
        std::vector<ActorId> actorsAlreadyDying;
        for (const Actor& actor : world.actors())
        {
            if (actor.life == LifeState::Dying)
            {
                actorsAlreadyDying.push_back(actor.id);
            }
        }
        applyDamageRequests(world, requests);

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
                requests.remove(id);
            }
        }
    }
}
