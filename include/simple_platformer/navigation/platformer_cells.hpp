#pragma once

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/navigation/route.hpp"

namespace simple_platformer
{
    class TileMap;

    // canOccupy for the cell's floor.
    bool canStandAt(const TileMap& map, Cell cell, glm::vec2 bodySize);

    // canOccupy for any of the cell's walls or its ceiling.
    bool canClimbAt(const TileMap& map, Cell cell, glm::vec2 bodySize);

    // The body's resting bounds at a location: standing in the cell, flush against
    // the cell's wall side, or hanging from the cell's top edge.
    Aabb boundsAtSurface(int tileSize, RouteLocation location, glm::vec2 bodySize);

    // Whether the body can rest at the location: the cell is on the map, the body fits
    // at its resting bounds there, and something holds it up. For the floor that is a
    // movement-blocking cell below; for a wall or the ceiling, a climbable tile the
    // resting bounds touch.
    bool canOccupy(const TileMap& map, RouteLocation location, glm::vec2 bodySize);
}
