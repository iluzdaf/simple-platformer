#pragma once

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/navigation/navigation_graph.hpp"

namespace simple_platformer
{
    class TileMap;

    // Whether the body can stand in the cell: the cell blocks nothing, nor does any cell
    // the body covers standing there, and the cell below blocks movement.
    bool canStandAt(const TileMap& map, GridPosition cell, glm::vec2 bodySize);

    // The body's resting bounds at a location: standing in the cell, flush against
    // the cell's wall side, or hanging from the cell's top edge.
    Aabb boundsAtSurface(int tileSize, NavigationLocation location, glm::vec2 bodySize);

    // Whether the body can rest at the location. The floor must be standable; a wall
    // or ceiling must be climbable where the resting bounds touch it.
    bool canOccupy(const TileMap& map, NavigationLocation location, glm::vec2 bodySize);
}
