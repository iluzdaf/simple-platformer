#include "simple_platformer/physics/segment_cast.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/validation.hpp"

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
}

namespace simple_platformer
{
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
}
