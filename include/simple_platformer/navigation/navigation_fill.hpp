#pragma once

namespace simple_platformer
{
    class PlatformerConnectionCache;
    struct FrameProfile;
    class TileMap;
    class World;

    // Background fill uses a queue per profile, never filling all cells at once during play.
    // Level start queues every map cell; breaks queue affected cells again. A fill each
    // simulation step caches some of them. Searches wait for queued cells they need.

    // Budget units are simulated movement ticks plus a fixed charge per cached cell,
    // shared among profiles with work waiting. A cell cannot be split across steps.
    constexpr int NavigationFillTicksPerStep = 250;

    // Queues every map cell for each distinct platformer NPC profile in the world.
    // Call once when the level starts.
    void queueNavigationFill(const TileMap& map, World& world, float stepSeconds);

    // Caches queued cells before NPCs think and returns how many were cached.
    // A cell cannot be split, so actual work may exceed the tick budget.
    int advanceNavigationFill(
        const TileMap& map,
        PlatformerConnectionCache& cache,
        int tickBudget,
        FrameProfile* frameProfile = nullptr);
}
