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

        enum class SurfaceKind
        {
            Blocking,
            Climbable
        };

        bool hasSurface(const TileMap& map, Cell cell, SurfaceKind kind)
        {
            return kind == SurfaceKind::Climbable ? map.climbableAt(cell)
                                                  : map.blocksMovement(cell);
        }

        bool columnHasSurface(
            const TileMap& map,
            int column,
            int firstRow,
            int lastRow,
            SurfaceKind kind)
        {
            for (int row = firstRow; row <= lastRow; ++row)
            {
                if (hasSurface(map, {column, row}, kind))
                {
                    return true;
                }
            }
            return false;
        }

        bool rowHasSurface(
            const TileMap& map,
            int row,
            int firstColumn,
            int lastColumn,
            SurfaceKind kind)
        {
            for (int column = firstColumn; column <= lastColumn; ++column)
            {
                if (hasSurface(map, {column, row}, kind))
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

        struct CollisionSweepResult
        {
            float distance = 0.0F;
            bool hitTile = false;
        };

        // Horizontal tile collision: scan columns nearest first, checking the body's rows.
        // Include the outside column at either map wall, but never scan beyond it.
        CollisionSweepResult sweepHorizontalCollision(
            const TileMap& map,
            const Aabb& bounds,
            float requested,
            SurfaceKind kind)
        {
            if (requested == 0.0F)
            {
                return {0.0F, false};
            }
            const int tileSize = map.tileSize();
            const int firstRow = std::max(0, firstOverlappingTile(tileSize, bounds.topLeft.y));
            const int lastRow =
                std::min(map.height() - 1, lastOverlappingTile(tileSize, bottomOf(bounds)));

            if (requested > 0.0F)
            {
                const float rightEdge = rightOf(bounds);
                const int firstColumn = firstOverlappingTile(tileSize, rightEdge);
                const float destination = rightEdge + requested;
                const int lastColumn = destination >= map.pixelWidth()
                                           ? map.width()
                                           : firstOverlappingTile(tileSize, destination);
                for (int column = firstColumn; column <= lastColumn; ++column)
                {
                    if (!columnHasSurface(map, column, firstRow, lastRow, kind))
                    {
                        continue;
                    }
                    const float tileLeft = static_cast<float>(column * tileSize);
                    const float distance = tileLeft - rightEdge;
                    if (stopsRequestedMovement(distance, requested))
                    {
                        return {distance, true};
                    }
                }
            }
            else
            {
                const float leftEdge = bounds.topLeft.x;
                const int firstColumn = lastOverlappingTile(tileSize, leftEdge);
                const float destination = leftEdge + requested;
                const int lastColumn =
                    destination <= 0.0F ? -1 : firstOverlappingTile(tileSize, destination);
                for (int column = firstColumn; column >= lastColumn; --column)
                {
                    if (!columnHasSurface(map, column, firstRow, lastRow, kind))
                    {
                        continue;
                    }
                    const float tileRight = static_cast<float>((column + 1) * tileSize);
                    const float distance = tileRight - leftEdge;
                    if (stopsRequestedMovement(distance, requested))
                    {
                        return {distance, true};
                    }
                }
            }
            return {requested, false};
        }

        // Vertical tile collision: scan rows nearest first, checking the body's columns.
        // The outside bottom row blocks movement; the outside top row remains open.
        CollisionSweepResult sweepVerticalCollision(
            const TileMap& map,
            const Aabb& bounds,
            float requested,
            SurfaceKind kind)
        {
            if (requested == 0.0F)
            {
                return {0.0F, false};
            }
            const int tileSize = map.tileSize();
            const int firstColumn = std::max(0, firstOverlappingTile(tileSize, bounds.topLeft.x));
            const int lastColumn =
                std::min(map.width() - 1, lastOverlappingTile(tileSize, rightOf(bounds)));

            if (requested > 0.0F)
            {
                const float bottomEdge = bottomOf(bounds);
                const int firstRow = firstOverlappingTile(tileSize, bottomEdge);
                const float destination = bottomEdge + requested;
                const int lastRow = destination >= map.pixelHeight()
                                        ? map.height()
                                        : firstOverlappingTile(tileSize, destination);
                for (int row = firstRow; row <= lastRow; ++row)
                {
                    if (!rowHasSurface(map, row, firstColumn, lastColumn, kind))
                    {
                        continue;
                    }
                    const float tileTop = static_cast<float>(row * tileSize);
                    const float distance = tileTop - bottomEdge;
                    if (stopsRequestedMovement(distance, requested))
                    {
                        return {distance, true};
                    }
                }
            }
            else
            {
                const float topEdge = bounds.topLeft.y;
                const int firstRow = lastOverlappingTile(tileSize, topEdge);
                const float destination = topEdge + requested;
                const int lastRow =
                    destination <= 0.0F ? -1 : firstOverlappingTile(tileSize, destination);
                for (int row = firstRow; row >= lastRow; --row)
                {
                    if (!rowHasSurface(map, row, firstColumn, lastColumn, kind))
                    {
                        continue;
                    }
                    const float tileBottom = static_cast<float>((row + 1) * tileSize);
                    const float distance = tileBottom - topEdge;
                    if (stopsRequestedMovement(distance, requested))
                    {
                        return {distance, true};
                    }
                }
            }
            return {requested, false};
        }

        // Reuse collision sweeps as short contact probes. Climbable selects grip
        // surfaces; ordinary movement always selects blocking surfaces.
        CollisionContacts probeSurfaces(const TileMap& map, const Aabb& bounds, SurfaceKind kind)
        {
            validateBounds(map, bounds, {0.0F, 0.0F});
            constexpr float ProbeDistance = 0.01F;
            return {
                sweepHorizontalCollision(map, bounds, -ProbeDistance, kind).hitTile,
                sweepHorizontalCollision(map, bounds, ProbeDistance, kind).hitTile,
                sweepVerticalCollision(map, bounds, ProbeDistance, kind).hitTile,
                sweepVerticalCollision(map, bounds, -ProbeDistance, kind).hitTile};
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
        const CollisionSweepResult horizontal =
            sweepHorizontalCollision(map, bounds, displacement.x, SurfaceKind::Blocking);
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

        const CollisionSweepResult vertical =
            sweepVerticalCollision(map, bounds, displacement.y, SurfaceKind::Blocking);
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
