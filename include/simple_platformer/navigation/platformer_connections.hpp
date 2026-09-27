#pragma once

#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_search.hpp"

namespace simple_platformer
{
    class PlatformerConnectionCache;
    class TileMap;

    // The connections leaving a cell, as a copy the caller owns: walks to every cell
    // along the floor either way, and the cheapest fall and jumps to either side that
    // land on a standable cell. With a cache, taken from it or kept in it as
    // platformerNeighborsKept does; without one, simulated for this call alone. With
    // statistics, adds the movement ticks simulated, or counts the cell as reused.
    std::vector<NavigationNeighbor> platformerNeighbors(
        const TileMap& map,
        GridPosition cell,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement,
        float stepSeconds,
        PathSearchStatistics* statistics = nullptr,
        PlatformerConnectionCache* cache = nullptr);

    // The connections leaving a cell, read from the cache rather than copied out of it:
    // simulated and kept first when the cache lacks them. The reference holds until a
    // break drops the cell or the cache is cleared. With statistics, counts the cell as
    // reused when it was kept already.
    const std::vector<NavigationNeighbor>& platformerNeighborsKept(
        const TileMap& map,
        GridPosition cell,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement,
        float stepSeconds,
        PlatformerConnectionCache& cache,
        PathSearchStatistics* statistics = nullptr);
}
