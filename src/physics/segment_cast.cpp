#include "simple_platformer/physics/segment_cast.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <stdexcept>

#include <glm/common.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    namespace
    {
        bool castAxis(
            float start,
            float movement,
            float minimum,
            float maximum,
            float& first,
            float& last)
        {
            if (movement == 0.0F)
            {
                return start >= minimum && start <= maximum;
            }

            float enter = (minimum - start) / movement;
            float leave = (maximum - start) / movement;
            if (enter > leave)
            {
                std::swap(enter, leave);
            }
            first = std::max(first, enter);
            last = std::min(last, leave);
            return first <= last;
        }

        Aabb expandedForMovingBox(const Aabb& target, glm::vec2 movingSize)
        {
            // Expand the target by half the moving box's size so we can cast
            // the box's centre as a line.
            const glm::vec2 halfSize = movingSize * 0.5F;
            return {target.position - halfSize, target.size + movingSize};
        }
    }

    std::optional<float> segmentCast(const Aabb& box, glm::vec2 start, glm::vec2 end)
    {
        if (!isFinite(box.position) || !isFinite(box.size) || !isFinite(start) || !isFinite(end) ||
            box.size.x <= 0.0F || box.size.y <= 0.0F)
        {
            throw std::invalid_argument("Segment casts require finite, positive-sized data");
        }

        const glm::vec2 movement = end - start;
        float first = 0.0F;
        float last = 1.0F;
        if (!castAxis(
                start.x, movement.x, box.position.x, box.position.x + box.size.x, first, last) ||
            !castAxis(
                start.y, movement.y, box.position.y, box.position.y + box.size.y, first, last))
        {
            return std::nullopt;
        }

        return first;
    }

    namespace
    {
        using TileBlockingQuery = std::function<bool(GridPosition)>;

        std::optional<TileSegmentHit> castTiles(
            glm::vec2 start,
            glm::vec2 end,
            glm::vec2 movingSize,
            const TileBlockingQuery& blocks)
        {
            if (!isFinite(start) || !isFinite(end) || !isFinite(movingSize) ||
                movingSize.x < 0.0F || movingSize.y < 0.0F)
            {
                throw std::invalid_argument(
                    "Tile segment casts require finite, non-negative-sized data");
            }

            const glm::vec2 halfSize = movingSize * 0.5F;
            const glm::vec2 minimum = glm::min(start, end) - halfSize;
            const glm::vec2 maximum = glm::max(start, end) + halfSize;
            const float tileSize = static_cast<float>(TileSize);
            const int firstColumn = static_cast<int>(std::floor(minimum.x / tileSize));
            const int lastColumn = static_cast<int>(std::floor(maximum.x / tileSize));
            const int firstRow = static_cast<int>(std::floor(minimum.y / tileSize));
            const int lastRow = static_cast<int>(std::floor(maximum.y / tileSize));

            std::optional<TileSegmentHit> earliest;
            for (int row = firstRow; row <= lastRow; ++row)
            {
                for (int column = firstColumn; column <= lastColumn; ++column)
                {
                    if (!blocks({column, row}))
                    {
                        continue;
                    }

                    const Aabb tile{
                        {static_cast<float>(column * TileSize), static_cast<float>(row * TileSize)},
                        {tileSize, tileSize}};
                    const std::optional<float> hit =
                        segmentCast(expandedForMovingBox(tile, movingSize), start, end);
                    if (hit.has_value() && (!earliest.has_value() || *hit < earliest->segmentTime))
                    {
                        earliest = TileSegmentHit{*hit, {column, row}};
                    }
                }
            }
            return earliest;
        }
    }

    std::optional<TileSegmentHit> segmentCastMovementBlockingTiles(
        const TileMap& map,
        glm::vec2 start,
        glm::vec2 end,
        glm::vec2 movingSize)
    {
        return castTiles(
            start, end, movingSize, [&map](GridPosition cell) { return map.blocksMovement(cell); });
    }

    std::optional<float> segmentCastSightBlockingTiles(
        const TileMap& map,
        glm::vec2 start,
        glm::vec2 end)
    {
        const std::optional<TileSegmentHit> hit = castTiles(
            start, end, {0.0F, 0.0F}, [&map](GridPosition cell) { return map.blocksSight(cell); });
        if (!hit.has_value())
        {
            return std::nullopt;
        }
        return hit->segmentTime;
    }
}
