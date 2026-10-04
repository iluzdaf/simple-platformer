#include "simple_platformer/physics/segment_cast.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <vector>

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
        // Narrows [first, last] to where the segment lies within this axis's bounds.
        // Returns false if it misses those bounds or no shared interval remains.
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

        // The part of the segment inside the box, as fractions along it.
        struct SegmentSpan
        {
            float enter = 0.0F;
            float leave = 0.0F;
        };

        std::optional<SegmentSpan> segmentSpan(const Aabb& box, glm::vec2 start, glm::vec2 end)
        {
            const glm::vec2 movement = end - start;
            float first = 0.0F;
            float last = 1.0F;
            if (!castAxis(start.x, movement.x, box.topLeft.x, rightOf(box), first, last) ||
                !castAxis(start.y, movement.y, box.topLeft.y, bottomOf(box), first, last))
            {
                return std::nullopt;
            }
            return SegmentSpan{first, last};
        }

        // Candidate cells for the segment, including the moving box's half-size.
        // Exact intersections are checked separately by each kind of cast.
        CellRange segmentTileRange(
            int tileSize,
            glm::vec2 start,
            glm::vec2 end,
            glm::vec2 movingSize)
        {
            if (!isFinite(start) || !isFinite(end) || !isFiniteNonNegative(movingSize))
            {
                throw std::invalid_argument(
                    "Tile segment casts require finite, non-negative-sized data");
            }
            const glm::vec2 halfSize = movingSize * 0.5F;
            const glm::vec2 minimum = glm::min(start, end) - halfSize;
            const glm::vec2 maximum = glm::max(start, end) + halfSize;
            return {cellAt(tileSize, minimum), cellAt(tileSize, maximum)};
        }

        Aabb tileBox(int tileSize, Cell cell)
        {
            const float tileLength = static_cast<float>(tileSize);
            return {cellCorner(tileSize, cell), {tileLength, tileLength}};
        }

        std::vector<SegmentSpan> sightBlockingSpans(
            const TileMap& map,
            glm::vec2 start,
            glm::vec2 end)
        {
            const CellRange cells = segmentTileRange(map.tileSize(), start, end, {0.0F, 0.0F});
            std::vector<SegmentSpan> spans;
            for (int row = cells.first.y; row <= cells.last.y; ++row)
            {
                for (int column = cells.first.x; column <= cells.last.x; ++column)
                {
                    const Cell cell{column, row};
                    if (!map.blocksSight(cell))
                    {
                        continue;
                    }
                    const auto span = segmentSpan(tileBox(map.tileSize(), cell), start, end);
                    if (span.has_value())
                    {
                        spans.push_back(*span);
                    }
                }
            }
            return spans;
        }

        // Spans that touch or overlap from time zero form the starting cover.
        // The first span after a gap blocks sight; an exact shared corner is not a gap.
        std::optional<float> firstHitAfterStartingCover(std::vector<SegmentSpan> spans)
        {
            std::sort(
                spans.begin(),
                spans.end(),
                [](const SegmentSpan& left, const SegmentSpan& right)
                { return left.enter < right.enter; });
            float startingCoverEnd = 0.0F;
            for (const SegmentSpan& span : spans)
            {
                if (span.enter > startingCoverEnd)
                {
                    return span.enter;
                }
                startingCoverEnd = std::max(startingCoverEnd, span.leave);
            }
            return std::nullopt;
        }
    }

    Aabb expandedForMovingBox(const Aabb& target, glm::vec2 movingSize)
    {
        const glm::vec2 halfSize = movingSize * 0.5F;
        return {target.topLeft - halfSize, target.size + movingSize};
    }

    std::optional<float> segmentCast(const Aabb& box, glm::vec2 start, glm::vec2 end)
    {
        if (!isFinite(box.topLeft) || !isFinite(box.size) || !isFinite(start) || !isFinite(end) ||
            box.size.x <= 0.0F || box.size.y <= 0.0F)
        {
            throw std::invalid_argument("Segment casts require finite, positive-sized data");
        }

        const std::optional<SegmentSpan> span = segmentSpan(box, start, end);
        if (!span.has_value())
        {
            return std::nullopt;
        }
        return span->enter;
    }

    std::optional<TileSegmentHit> segmentCastMovementBlockingTiles(
        const TileMap& map,
        glm::vec2 start,
        glm::vec2 end,
        glm::vec2 movingSize)
    {
        const CellRange cells = segmentTileRange(map.tileSize(), start, end, movingSize);
        std::optional<TileSegmentHit> earliest;
        for (int row = cells.first.y; row <= cells.last.y; ++row)
        {
            for (int column = cells.first.x; column <= cells.last.x; ++column)
            {
                const Cell cell{column, row};
                if (!map.blocksMovement(cell))
                {
                    continue;
                }
                const Aabb target = expandedForMovingBox(tileBox(map.tileSize(), cell), movingSize);
                const auto hit = segmentCast(target, start, end);
                if (hit.has_value() && (!earliest.has_value() || *hit < earliest->segmentTime))
                {
                    earliest = TileSegmentHit{*hit, cell};
                }
            }
        }
        return earliest;
    }

    std::optional<float> segmentCastSightBlockingTiles(
        const TileMap& map,
        glm::vec2 start,
        glm::vec2 end)
    {
        return firstHitAfterStartingCover(sightBlockingSpans(map, start, end));
    }
}
