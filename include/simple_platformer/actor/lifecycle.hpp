#pragma once

#include <vector>

#include "simple_platformer/actor/actor.hpp"

namespace simple_platformer
{
    class World;

    struct DamageRequest
    {
        ActorId target;
        int amount = 0;
    };

    class WorldRequests
    {
    public:
        void damage(ActorId target, int amount);
        void remove(ActorId target);
        bool empty() const;

    private:
        friend void updateLifeState(World&, WorldRequests&, float, float);

        std::vector<DamageRequest> damageRequests;
        std::vector<ActorId> removalRequests;
    };

    void updateLifeState(
        World& world,
        WorldRequests& requests,
        float deltaTime,
        float deathDuration = 0.4F);
}
