#pragma once

#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

namespace simple_platformer
{
    class TileMap;

    bool canStandAt(const TileMap& map, GridPosition position, glm::vec2 bodySize);

    std::vector<NavigationNeighbor> platformerNeighbors(
        const TileMap& map,
        GridPosition position,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement);
}
