#pragma once

#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

namespace simple_platformer
{
    class TileMap;

    // Optimistic remaining travel time in fixed simulation ticks.
    int platformerTickHeuristic(
        GridPosition position,
        GridPosition goal,
        const PlatformerMovementConfig& movement);

    // High-level path API for platformer actors.
    std::optional<NavigationPath> findPlatformerPath(
        const TileMap& map,
        GridPosition start,
        GridPosition goal,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement);

    bool canStandAt(const TileMap& map, GridPosition position, glm::vec2 bodySize);

    // Lower-level policy used by the generic path search.
    std::vector<NavigationNeighbor> platformerNeighbors(
        const TileMap& map,
        GridPosition position,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement);
}
