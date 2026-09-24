#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"

namespace simple_platformer
{
    class TileMap;
    class World;

    // One cell a body can stand in, as the connection cache sees it: how many connections
    // it keeps for the cell, or nothing while it keeps none, as after a break drops them.
    struct NavigationCellDebugInfo
    {
        Aabb bounds;
        std::optional<std::size_t> connections;
    };

    // The connection cache's view of the map for one platformer NPC body, the first in
    // the world: every cell that body can stand in. Absent without such an NPC.
    struct NavigationCacheDebugInfo
    {
        glm::vec2 bodySize = {0.0F, 0.0F};
        std::vector<NavigationCellDebugInfo> cells;
    };

    // Built from the map and the world's connection cache, without ImGui, so it can be
    // tested. The step is the one the world is simulated with, which is part of the
    // body the cache keys on.
    std::optional<NavigationCacheDebugInfo> makeNavigationCacheDebugInfo(
        const World& world,
        const TileMap& map,
        float simulationStepSeconds);
}
