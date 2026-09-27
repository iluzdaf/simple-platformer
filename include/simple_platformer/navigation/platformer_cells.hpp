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

    // Finds the closest standable cell beneath a grounded body. The body's feet may
    // extend beyond a ledge while part of its collider is still supported.
    std::optional<GridPosition> findPlatformerStartCell(const TileMap& map, const Aabb& bounds);

    // Keeps a standable target cell; otherwise chooses the closest standable feet
    // position for this NPC's body size. Ties use row, then column order.
    // This selects a chase destination, not a guaranteed path to it.
    std::optional<GridPosition> findPlatformerChaseCell(
        const TileMap& map,
        glm::vec2 lastKnownFeet,
        glm::vec2 bodySize);
}
