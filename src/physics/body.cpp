#include "simple_platformer/physics/body.hpp"

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
            if (!isFinite(bounds.topLeft) || !isFinite(bounds.size) || !isFinite(displacement) ||
                bounds.size.x <= 0.0F || bounds.size.y <= 0.0F)
            {
                throw std::invalid_argument("Collision requires finite, positive-sized bounds");
            }

            if (bounds.topLeft.x < 0.0F || rightOf(bounds) > map.pixelWidth() ||
                bottomOf(bounds) > map.pixelHeight())
            {
                throw std::invalid_argument("Collision bounds must begin inside the map walls");
            }
        }

        enum class Axis
        {
            Horizontal,
            Vertical
        };

        enum class SurfaceKind
        {
            Blocking,
            Climbable
        };

        bool lineHasSurface(
            const TileMap& map,
            Axis axis,
            int along,
            int firstAcross,
            int lastAcross,
            SurfaceKind kind)
        {
            for (int across = firstAcross; across <= lastAcross; ++across)
            {
                Cell cell;
                if (axis == Axis::Horizontal)
                {
                    cell = {along, across}; // Scan a column, from top to bottom.
                }
                else
                {
                    cell = {across, along}; // Scan a row, from left to right.
                }
                if (kind == SurfaceKind::Climbable ? map.climbableAt(cell)
                                                   : map.blocksMovement(cell))
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

        struct SweepResult
        {
            float distance = 0.0F;
            bool hitTile = false;
        };

        // Returns how far the box can move along one axis before hitting the requested
        // kind of surface. Scans from the leading edge to its proposed position, nearest
        // tiles first. At a map edge it queries the outside cell too, so blocking boundaries
        // stop the box and the open top lets it pass.
        SweepResult sweepAxis(
            const TileMap& map,
            Axis axis,
            const Aabb& bounds,
            float requested,
            SurfaceKind kind)
        {
            if (requested == 0.0F)
            {
                return {0.0F, false};
            }

            const bool forward = requested > 0.0F;
            float leadingEdge;
            float acrossMinimum;
            float acrossMaximum;
            float mapExtent;
            int alongCount;
            int acrossCount;
            if (axis == Axis::Horizontal)
            {
                leadingEdge = forward ? rightOf(bounds) : bounds.topLeft.x;
                acrossMinimum = bounds.topLeft.y;
                acrossMaximum = bottomOf(bounds);
                mapExtent = map.pixelWidth();
                alongCount = map.width();
                acrossCount = map.height();
            }
            else
            {
                leadingEdge = forward ? bottomOf(bounds) : bounds.topLeft.y;
                acrossMinimum = bounds.topLeft.x;
                acrossMaximum = rightOf(bounds);
                mapExtent = map.pixelHeight();
                alongCount = map.height();
                acrossCount = map.width();
            }

            // Only rows (for X) or columns (for Y) overlapped by the box can stop it.
            const int firstAcross =
                std::max(0, firstOverlappingTile(map.tileSize(), acrossMinimum));
            const int lastAcross =
                std::min(acrossCount - 1, lastOverlappingTile(map.tileSize(), acrossMaximum));
            const int firstAlong = forward ? firstOverlappingTile(map.tileSize(), leadingEdge)
                                           : lastOverlappingTile(map.tileSize(), leadingEdge);
            const float finalLeadingEdge = leadingEdge + requested;
            int lastAlong;
            if (forward && finalLeadingEdge >= mapExtent)
            {
                lastAlong = alongCount;
            }
            else if (!forward && finalLeadingEdge <= 0.0F)
            {
                lastAlong = -1;
            }
            else
            {
                lastAlong = firstOverlappingTile(map.tileSize(), finalLeadingEdge);
            }
            const int step = forward ? 1 : -1;

            for (int along = firstAlong; forward ? along <= lastAlong : along >= lastAlong;
                 along += step)
            {
                if (!lineHasSurface(map, axis, along, firstAcross, lastAcross, kind))
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

        CollisionContacts probeSurfaces(const TileMap& map, const Aabb& bounds, SurfaceKind kind)
        {
            validateBounds(map, bounds, {0.0F, 0.0F});
            constexpr float ProbeDistance = 0.01F;
            return {
                sweepAxis(map, Axis::Horizontal, bounds, -ProbeDistance, kind).hitTile,
                sweepAxis(map, Axis::Horizontal, bounds, ProbeDistance, kind).hitTile,
                sweepAxis(map, Axis::Vertical, bounds, ProbeDistance, kind).hitTile,
                sweepAxis(map, Axis::Vertical, bounds, -ProbeDistance, kind).hitTile};
        }
    }

    void applyGravity(Body& body, float gravity, float maximumFallSpeed, float deltaTime)
    {
        body.velocity.y = std::min(body.velocity.y + gravity * deltaTime, maximumFallSpeed);
    }

    CollisionContacts moveBody(const TileMap& map, Body& body, float deltaTime)
    {
        Aabb& bounds = body.bounds;
        const glm::vec2 displacement = body.velocity * deltaTime;
        validateBounds(map, bounds, displacement);

        CollisionContacts contacts;
        // Resolve X first, then sweep Y from the new horizontal position.
        const SweepResult horizontal =
            sweepAxis(map, Axis::Horizontal, bounds, displacement.x, SurfaceKind::Blocking);
        bounds.topLeft.x += horizontal.distance;
        if (horizontal.hitTile)
        {
            if (displacement.x > 0.0F)
            {
                contacts.right = true;
            }
            else
            {
                contacts.left = true;
            }
            body.velocity.x = 0.0F;
        }

        const SweepResult vertical =
            sweepAxis(map, Axis::Vertical, bounds, displacement.y, SurfaceKind::Blocking);
        bounds.topLeft.y += vertical.distance;
        if (vertical.hitTile)
        {
            if (displacement.y > 0.0F)
            {
                contacts.ground = true;
            }
            else
            {
                contacts.ceiling = true;
            }
            body.velocity.y = 0.0F;
        }
        return contacts;
    }

    CollisionContacts touchingSurfaces(const TileMap& map, const Aabb& bounds)
    {
        return probeSurfaces(map, bounds, SurfaceKind::Blocking);
    }

    CollisionContacts touchingClimbableSurfaces(const TileMap& map, const Aabb& bounds)
    {
        return probeSurfaces(map, bounds, SurfaceKind::Climbable);
    }
}
