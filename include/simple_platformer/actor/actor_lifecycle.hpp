#pragma once

namespace simple_platformer
{
    class World;
    class WorldRequests;

    constexpr float ActorDeathSeconds = 0.4F;

    // Applies queued damage and begins fatal deaths without advancing timers.
    void applyDamageRequests(World& world, WorldRequests& requests);

    // Applies damage, then advances only deaths that existed before this update.
    // Expired deaths respawn the player or queue NPC removal.
    void updateActorLifecycle(World& world, WorldRequests& requests, float deltaTime);
}
