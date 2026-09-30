#include "simple_platformer/navigation/platformer_cells.hpp"

#include <stdexcept>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/navigation/route.hpp"
#include "simple_platformer/physics/collision.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    namespace
    {
        // Clear of movement-blocking tiles and inside the map's blocking sides and
        // bottom. The map is open above its top edge, as it is for collision.
        bool bodyFits(const TileMap& map, const Aabb& bounds)
        {
            if (bounds.position.x < 0.0F || rightOf(bounds) > map.pixelWidth() ||
                bottomOf(bounds) > map.pixelHeight())
            {
                return false;
            }
            const CellRange cells = cellsCovered(map.tileSize(), bounds);
            for (int row = cells.first.y; row <= cells.last.y; ++row)
            {
                for (int column = cells.first.x; column <= cells.last.x; ++column)
                {
                    if (map.blocksMovement({column, row}))
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        void requireBodySize(glm::vec2 bodySize)
        {
            if (!isFinite(bodySize) || bodySize.x <= 0.0F || bodySize.y <= 0.0F)
            {
                throw std::invalid_argument("Navigation body size must be finite and positive");
            }
        }

        // canStandAt without the size check, for callers that have made it.
        bool standsAt(const TileMap& map, Cell cell, glm::vec2 bodySize)
        {
            return map.contains(cell) && !map.blocksMovement(cell) &&
                   map.blocksMovement({cell.x, cell.y + 1}) &&
                   bodyFits(map, boxInCell(map.tileSize(), cell, bodySize));
        }
    }

    bool canStandAt(const TileMap& map, Cell cell, glm::vec2 bodySize)
    {
        requireBodySize(bodySize);
        return standsAt(map, cell, bodySize);
    }

    Aabb boundsAtSurface(int tileSize, RouteLocation location, glm::vec2 bodySize)
    {
        Aabb bounds = boxInCell(tileSize, location.cell, bodySize);
        const float left = static_cast<float>(location.cell.x * tileSize);
        const float top = static_cast<float>(location.cell.y * tileSize);
        switch (location.surface)
        {
        case ClimbSurface::None:
            break;
        case ClimbSurface::LeftWall:
            bounds.position.x = left;
            break;
        case ClimbSurface::RightWall:
            bounds.position.x = left + static_cast<float>(tileSize) - bodySize.x;
            break;
        case ClimbSurface::Ceiling:
            bounds.position.y = top;
            break;
        }
        return bounds;
    }

    bool canOccupy(const TileMap& map, RouteLocation location, glm::vec2 bodySize)
    {
        requireBodySize(bodySize);
        if (location.surface == ClimbSurface::None)
        {
            return standsAt(map, location.cell, bodySize);
        }
        if (!map.contains(location.cell))
        {
            return false;
        }
        const Aabb bounds = boundsAtSurface(map.tileSize(), location, bodySize);
        return bodyFits(map, bounds) &&
               touchesSurface(location.surface, touchingClimbableSurfaces(map, bounds));
    }
}
