#include "simple_platformer/physics/collision.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace
{
    int firstOverlappingTile(float minimum)
    {
        return static_cast<int>(
            std::floor(minimum / static_cast<float>(simple_platformer::TileSize)));
    }

    int lastOverlappingTile(float maximum)
    {
        return static_cast<int>(
                   std::ceil(maximum / static_cast<float>(simple_platformer::TileSize))) -
               1;
    }

    void validateBounds(
        const simple_platformer::TileMap& map,
        const simple_platformer::Aabb& bounds,
        glm::vec2 displacement)
    {
        if (!simple_platformer::isFinite(bounds.position) ||
            !simple_platformer::isFinite(bounds.size) ||
            !simple_platformer::isFinite(displacement) || bounds.size.x <= 0.0F ||
            bounds.size.y <= 0.0F)
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

    bool columnBlocksMovement(
        const simple_platformer::TileMap& map,
        int column,
        int firstRow,
        int lastRow)
    {
        for (int row = firstRow; row <= lastRow; ++row)
        {
            if (map.blocksMovement({column, row}))
            {
                return true;
            }
        }

        return false;
    }

    bool rowBlocksMovement(
        const simple_platformer::TileMap& map,
        int row,
        int firstColumn,
        int lastColumn)
    {
        for (int column = firstColumn; column <= lastColumn; ++column)
        {
            if (map.blocksMovement({column, row}))
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

    float allowedHorizontalMovement(
        const simple_platformer::TileMap& map,
        const simple_platformer::Aabb& bounds,
        float requested,
        simple_platformer::CollisionContacts& contacts)
    {
        if (requested == 0.0F)
        {
            return 0.0F;
        }

        const bool movingRight = requested > 0.0F;
        const float leadingEdge = bounds.position.x + (movingRight ? bounds.size.x : 0.0F);
        const int firstRow = std::max(0, firstOverlappingTile(bounds.position.y));
        const int lastRow =
            std::min(map.height() - 1, lastOverlappingTile(bounds.position.y + bounds.size.y));
        const int firstColumn =
            movingRight ? firstOverlappingTile(leadingEdge) : lastOverlappingTile(leadingEdge);
        const float finalLeadingEdge = leadingEdge + requested;
        const int lastColumn = movingRight && finalLeadingEdge >= map.pixelWidth() ? map.width()
                               : !movingRight && finalLeadingEdge <= 0.0F
                                   ? -1
                                   : firstOverlappingTile(finalLeadingEdge);
        const int step = movingRight ? 1 : -1;

        for (int column = firstColumn; movingRight ? column <= lastColumn : column >= lastColumn;
             column += step)
        {
            if (!columnBlocksMovement(map, column, firstRow, lastRow))
            {
                continue;
            }

            const int tileEdge = movingRight ? column : column + 1;
            const float candidate =
                static_cast<float>(tileEdge * simple_platformer::TileSize) - leadingEdge;
            if (!stopsRequestedMovement(candidate, requested))
            {
                continue;
            }

            if (movingRight)
            {
                contacts.right = true;
            }
            else
            {
                contacts.left = true;
            }

            return candidate;
        }

        return requested;
    }

    float allowedVerticalMovement(
        const simple_platformer::TileMap& map,
        const simple_platformer::Aabb& bounds,
        float requested,
        simple_platformer::CollisionContacts& contacts)
    {
        if (requested == 0.0F)
        {
            return 0.0F;
        }

        const bool movingDown = requested > 0.0F;
        const float leadingEdge = bounds.position.y + (movingDown ? bounds.size.y : 0.0F);
        const int firstColumn = std::max(0, firstOverlappingTile(bounds.position.x));
        const int lastColumn =
            std::min(map.width() - 1, lastOverlappingTile(bounds.position.x + bounds.size.x));
        const int firstRow =
            movingDown ? firstOverlappingTile(leadingEdge) : lastOverlappingTile(leadingEdge);
        const float finalLeadingEdge = leadingEdge + requested;
        const int lastRow = movingDown && finalLeadingEdge >= map.pixelHeight() ? map.height()
                            : !movingDown && finalLeadingEdge < 0.0F
                                ? -1
                                : firstOverlappingTile(finalLeadingEdge);
        const int step = movingDown ? 1 : -1;

        for (int row = firstRow; movingDown ? row <= lastRow : row >= lastRow; row += step)
        {
            if (!rowBlocksMovement(map, row, firstColumn, lastColumn))
            {
                continue;
            }

            const int tileEdge = movingDown ? row : row + 1;
            const float candidate =
                static_cast<float>(tileEdge * simple_platformer::TileSize) - leadingEdge;
            if (!stopsRequestedMovement(candidate, requested))
            {
                continue;
            }

            if (movingDown)
            {
                contacts.ground = true;
            }
            else
            {
                contacts.ceiling = true;
            }

            return candidate;
        }

        return requested;
    }
}

namespace simple_platformer
{
    CollisionContacts moveAndCollide(const TileMap& map, Aabb& bounds, glm::vec2 displacement)
    {
        validateBounds(map, bounds, displacement);

        CollisionContacts contacts;
        bounds.position.x += allowedHorizontalMovement(map, bounds, displacement.x, contacts);
        bounds.position.y += allowedVerticalMovement(map, bounds, displacement.y, contacts);
        return contacts;
    }
}
