#pragma once

namespace simple_platformer
{
    class World;
    class WorldRequests;

    // Consumes damage requests and starts death timers. Only actors already dying at
    // entry have their timers advanced, so a new death keeps its full duration. Expiry
    // respawns the player or queues NPC removal.
    void updateLifeState(
        World& world,
        WorldRequests& requests,
        float deltaTime,
        float deathDuration = 0.4F);
}
