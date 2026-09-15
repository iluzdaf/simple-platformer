#pragma once

#include <cstdint>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"

namespace simple_platformer
{
    class World
    {
    public:
        ActorId addActor(Actor actor);
        bool removeActor(ActorId id);

        // Adding or removing actors invalidates returned pointers. Keep ActorId values across
        // world mutations and use these pointers only for temporary access.
        Actor* findActor(ActorId id);
        const Actor* findActor(ActorId id) const;

        std::vector<Actor>& actors();
        const std::vector<Actor>& actors() const;

        void setPlayer(ActorId id, glm::vec2 spawnFeet);
        ActorId playerId() const;
        glm::vec2 playerSpawnFeet() const;
        void respawnPlayer();

    private:
        std::vector<Actor> actorStorage;
        std::uint32_t nextActorId = 1;
        ActorId controlledPlayer;
        glm::vec2 controlledPlayerSpawnFeet = {0.0F, 0.0F};
    };
}
