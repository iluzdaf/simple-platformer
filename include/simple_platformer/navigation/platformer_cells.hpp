#pragma once

#include <optional>
#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"

namespace simple_platformer
{
    struct Aabb;
    class TileMap;

    // Whether the body can stand in the cell: the cell blocks nothing, nor does any cell
    // the body covers standing there, and the cell below blocks movement.
    bool canStandAt(const TileMap& map, GridPosition cell, glm::vec2 bodySize);

    // Maps a grounded body's collider to a standable cell at its feet row. At a ledge,
    // its feet may extend past the cell that still supports it.
    std::optional<GridPosition> findPlatformerStartCell(const TileMap& map, const Aabb& bounds);

    // Keeps the feet's cell if standable; otherwise chooses the closest standable
    // cell for this body size. Ties use row, then column order. Reachability is not checked.
    std::optional<GridPosition> findNearestStandableCell(
        const TileMap& map,
        glm::vec2 targetFeet,
        glm::vec2 bodySize);
}
