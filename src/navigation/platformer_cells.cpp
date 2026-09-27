#include "simple_platformer/navigation/platformer_cells.hpp"

#include <algorithm>
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

        // Nearest first: rings of cells around the cell nearest the feet, out until a ring
        // can no longer beat the best found. Every cell in ring r lies at least r - 1
        // tiles from the feet, so once that exceeds the best distance the rest of the map
        // cannot win. The closest feet position wins; equal distances keep row, then
        // column order, as a scan of the whole map would.
        const auto tileSize = static_cast<float>(map.tileSize());
        const GridPosition anchor{
            static_cast<int>(
                std::floor(std::clamp(lastKnownFeet.x, 0.0F, map.pixelWidth() - 1.0F) / tileSize)),
            static_cast<int>(std::floor(
                std::clamp(lastKnownFeet.y, 0.0F, map.pixelHeight() - 1.0F) / tileSize))};
        const int farthestRing =
            std::max({anchor.x, map.width() - 1 - anchor.x, anchor.y, map.height() - 1 - anchor.y});

        std::optional<GridPosition> closest;
        double closestDistanceSquared = 0.0;
        const auto consider = [&](GridPosition candidate)
        {
            if (!canStandAt(map, candidate, bodySize))
            {
                return;
            }
            const glm::vec2 candidateFeet = feetInCell(map.tileSize(), candidate);
            const double dx = static_cast<double>(candidateFeet.x) - lastKnownFeet.x;
            const double dy = static_cast<double>(candidateFeet.y) - lastKnownFeet.y;
            const double distanceSquared = dx * dx + dy * dy;
            const bool earlierInScanOrder =
                closest.has_value() && (candidate.y < closest->y ||
                                        (candidate.y == closest->y && candidate.x < closest->x));
            if (!closest.has_value() || distanceSquared < closestDistanceSquared ||
                (distanceSquared == closestDistanceSquared && earlierInScanOrder))
            {
                closest = candidate;
                closestDistanceSquared = distanceSquared;
            }
        };

        for (int ring = 0; ring <= farthestRing; ++ring)
        {
            if (closest.has_value())
            {
                const double nearestPossible = static_cast<double>(ring - 1) * tileSize;
                if (nearestPossible > 0.0 &&
                    nearestPossible * nearestPossible > closestDistanceSquared)
                {
                    break;
                }
            }
            if (ring == 0)
            {
                consider(anchor);
                continue;
            }
            for (int column = anchor.x - ring; column <= anchor.x + ring; ++column)
            {
                consider({column, anchor.y - ring});
                consider({column, anchor.y + ring});
            }
            for (int row = anchor.y - ring + 1; row <= anchor.y + ring - 1; ++row)
            {
                consider({anchor.x - ring, row});
                consider({anchor.x + ring, row});
            }
        }
        return closest;
    }
}
