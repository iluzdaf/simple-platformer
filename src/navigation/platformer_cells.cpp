#include "simple_platformer/navigation/platformer_cells.hpp"

#include <cmath>
#include <optional>
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
        bool bodyFits(const TileMap& map, const Aabb& bounds)
        {
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

        std::optional<GridPosition> nearestStandableChaseCell(
            const TileMap& map,
            glm::vec2 targetFeet,
            glm::vec2 bodySize)
        {
            std::optional<GridPosition> closest;
            double closestDistanceSquared = 0.0;
            for (int row = 0; row < map.height(); ++row)
            {
                for (int column = 0; column < map.width(); ++column)
                {
                    const GridPosition candidate{column, row};
                    if (!canStandAt(map, candidate, bodySize))
                    {
                        continue;
                    }
                    const glm::vec2 candidateFeet = feetInCell(map.tileSize(), candidate);
                    const double dx = static_cast<double>(candidateFeet.x) - targetFeet.x;
                    const double dy = static_cast<double>(candidateFeet.y) - targetFeet.y;
                    const double distanceSquared = dx * dx + dy * dy;
                    // Row-major order makes an equal-distance candidate keep the first cell.
                    if (!closest.has_value() || distanceSquared < closestDistanceSquared)
                    {
                        closest = candidate;
                        closestDistanceSquared = distanceSquared;
                    }
                }
            }
            return closest;
        }
    }

    bool canStandAt(const TileMap& map, GridPosition cell, glm::vec2 bodySize)
    {
        if (!isFinite(bodySize) || bodySize.x <= 0.0F || bodySize.y <= 0.0F)
        {
            throw std::invalid_argument("Navigation body size must be finite and positive");
        }
        return map.contains(cell) && !map.blocksMovement(cell) &&
               map.blocksMovement({cell.x, cell.y + 1}) &&
               bodyFits(map, boxInCell(map.tileSize(), cell, bodySize));
    }

    std::optional<GridPosition> findPlatformerStartCell(const TileMap& map, const Aabb& bounds)
    {
        if (!isFinite(bounds.position) || !isFinite(bounds.size) || bounds.size.x <= 0.0F ||
            bounds.size.y <= 0.0F)
        {
            throw std::invalid_argument(
                "A platformer navigation body must have finite, positive-sized bounds");
        }

        const glm::vec2 feet = feetOf(bounds);
        const GridPosition feetCell = cellAtFeet(map.tileSize(), feet);
        if (canStandAt(map, feetCell, bounds.size))
        {
            return feetCell;
        }

        const CellRange cells = cellsCovered(map.tileSize(), bounds);
        std::optional<GridPosition> closest;
        float closestDistance = 0.0F;
        for (int column = cells.first.x; column <= cells.last.x; ++column)
        {
            const GridPosition candidate{column, feetCell.y};
            if (!canStandAt(map, candidate, bounds.size))
            {
                continue;
            }

            const float distance = std::abs(feetInCell(map.tileSize(), candidate).x - feet.x);
            if (!closest.has_value() || distance < closestDistance)
            {
                closest = candidate;
                closestDistance = distance;
            }
        }
        return closest;
    }

    std::optional<GridPosition> findPlatformerChaseCell(
        const TileMap& map,
        glm::vec2 lastKnownFeet,
        glm::vec2 bodySize)
    {
        if (!isFinite(lastKnownFeet) || !isFinite(bodySize) || bodySize.x <= 0.0F ||
            bodySize.y <= 0.0F)
        {
            throw std::invalid_argument(
                "A chase destination requires finite feet and a finite, positive body size");
        }

        // Avoid converting an out-of-map world position to an integer grid cell.
        if (lastKnownFeet.x >= 0.0F && lastKnownFeet.x < map.pixelWidth() &&
            lastKnownFeet.y >= 0.0F && lastKnownFeet.y <= map.pixelHeight())
        {
            const GridPosition targetCell = cellAtFeet(map.tileSize(), lastKnownFeet);
            if (canStandAt(map, targetCell, bodySize))
            {
                return targetCell;
            }
        }

        return nearestStandableChaseCell(map, lastKnownFeet, bodySize);
    }
}
