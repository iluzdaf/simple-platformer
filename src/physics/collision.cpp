#include "simple_platformer/physics/collision.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    namespace
    {
        int firstOverlappingTile(int tileSize, float minimum)
        {
            return static_cast<int>(std::floor(minimum / static_cast<float>(tileSize)));
        }

        int lastOverlappingTile(int tileSize, float maximum)
        {
            return static_cast<int>(std::ceil(maximum / static_cast<float>(tileSize))) - 1;
        }

        void validateBounds(const TileMap& map, const Aabb& bounds, glm::vec2 displacement)
        {
            if (!isFinite(bounds.position) || !isFinite(bounds.size) || !isFinite(displacement) ||
                bounds.size.x <= 0.0F || bounds.size.y <= 0.0F)
            {
                throw std::invalid_argument("Collision requires finite, positive-sized bounds");
            }

            const float right = bounds.position.x + bounds.size.x;
            const float bottom = bounds.position.y + bounds.size.y;
            if (bounds.position.x < 0.0F || right > map.pixelWidth() || bottom > map.pixelHeight())
            {
                throw std::invalid_argument("Collision bounds must begin inside the map walls");
            }
        }

        // One axis of the map as the movement sees it: the extent and cell count along the
        // way, and how many cells lie across it. x is axis 0 and y is axis 1, as in glm.
        struct AxisView
        {
            int axis = 0;
            float alongExtent = 0.0F;
            int alongCount = 0;
            int acrossCount = 0;

            GridPosition cell(int along, int across) const
            {
                return axis == 0 ? GridPosition{along, across} : GridPosition{across, along};
            }
        };

        AxisView axisView(const TileMap& map, int axis)
        {
            return axis == 0 ? AxisView{0, map.pixelWidth(), map.width(), map.height()}
                             : AxisView{1, map.pixelHeight(), map.height(), map.width()};
        }

        bool lineBlocksMovement(
            const TileMap& map,
            const AxisView& view,
            int along,
            int firstAcross,
            int lastAcross)
        {
            for (int across = firstAcross; across <= lastAcross; ++across)
            {
                if (map.blocksMovement(view.cell(along, across)))
                {
                    return true;
                }
            }
            return false;
        }

        bool stopsRequestedMovement(float candidate, float requested)
        {
            if (requested > 0.0F)
            {
                return candidate >= 0.0F && candidate <= requested;
            }

            return candidate <= 0.0F && candidate >= requested;
        }

        struct AllowedMovement
        {
            float distance = 0.0F;
            bool hitTile = false;
        };

        // How far the box may move along one axis before a blocking tile stops it. The scan
        // runs from the cell the leading edge is in to the cell it would end in. Arriving
        // exactly at a map edge counts as reaching the cell beyond it: past the sides and
        // bottom that cell blocks, and above the top it is open, so either way the answer
        // is the map's.
        AllowedMovement allowedMovement(
            const TileMap& map,
            const AxisView& view,
            const Aabb& bounds,
            float requested)
        {
            if (requested == 0.0F)
            {
                return {0.0F, false};
            }

            const int across = 1 - view.axis;
            const bool forward = requested > 0.0F;
            const float leadingEdge =
                bounds.position[view.axis] + (forward ? bounds.size[view.axis] : 0.0F);
            const int firstAcross =
                std::max(0, firstOverlappingTile(map.tileSize(), bounds.position[across]));
            const int lastAcross = std::min(
                view.acrossCount - 1,
                lastOverlappingTile(map.tileSize(), bounds.position[across] + bounds.size[across]));
            const int firstAlong = forward ? firstOverlappingTile(map.tileSize(), leadingEdge)
                                           : lastOverlappingTile(map.tileSize(), leadingEdge);
            const float finalLeadingEdge = leadingEdge + requested;
            const int lastAlong = forward && finalLeadingEdge >= view.alongExtent ? view.alongCount
                                  : !forward && finalLeadingEdge <= 0.0F
                                      ? -1
                                      : firstOverlappingTile(map.tileSize(), finalLeadingEdge);
            const int step = forward ? 1 : -1;

            for (int along = firstAlong; forward ? along <= lastAlong : along >= lastAlong;
                 along += step)
            {
                if (!lineBlocksMovement(map, view, along, firstAcross, lastAcross))
                {
                    continue;
                }

                const int tileEdge = forward ? along : along + 1;
                const float candidate = static_cast<float>(tileEdge * map.tileSize()) - leadingEdge;
                if (stopsRequestedMovement(candidate, requested))
                {
                    return {candidate, true};
                }
            }

            return {requested, false};
        }
    }

    CollisionContacts moveAndCollide(const TileMap& map, Aabb& bounds, glm::vec2 displacement)
    {
        validateBounds(map, bounds, displacement);

        CollisionContacts contacts;
        const AllowedMovement horizontal =
            allowedMovement(map, axisView(map, 0), bounds, displacement.x);
        bounds.position.x += horizontal.distance;
        if (horizontal.hitTile)
        {
            (displacement.x > 0.0F ? contacts.right : contacts.left) = true;
        }

        const AllowedMovement vertical =
            allowedMovement(map, axisView(map, 1), bounds, displacement.y);
        bounds.position.y += vertical.distance;
        if (vertical.hitTile)
        {
            (displacement.y > 0.0F ? contacts.ground : contacts.ceiling) = true;
        }
        return contacts;
    }
}
