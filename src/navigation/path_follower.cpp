#include "simple_platformer/navigation/path_follower.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"

namespace simple_platformer
{
    GridPosition navigationCell(glm::vec2 feet)
    {
        constexpr float BoundaryOffset = 0.001F;
        return worldToGrid({feet.x, feet.y - BoundaryOffset});
    }

    glm::vec2 navigationFeet(GridPosition cell)
    {
        const glm::vec2 topLeft = gridToWorld(cell);
        return {
            topLeft.x + static_cast<float>(TileSize) * 0.5F,
            topLeft.y + static_cast<float>(TileSize)};
    }

    void setPath(PathFollower& follower, std::vector<GridPosition> path, GridPosition destination)
    {
        if (path.empty())
        {
            throw std::invalid_argument("A path follower cannot follow an empty path");
        }
        follower.path = std::move(path);
        follower.nextStep = follower.path.size() > 1 ? 1 : follower.path.size();
        follower.destination = destination;
    }

    void clearPath(PathFollower& follower)
    {
        follower.path.clear();
        follower.nextStep = 0;
        follower.destination.reset();
    }

    bool pathComplete(const PathFollower& follower)
    {
        return !follower.path.empty() && follower.nextStep >= follower.path.size();
    }

    InputIntentions followFlyingPath(const Aabb& bounds, PathFollower& follower)
    {
        constexpr float ArrivalDistance = 1.0F;
        const glm::vec2 feet = feetOf(bounds);
        while (follower.nextStep < follower.path.size())
        {
            const glm::vec2 offset = navigationFeet(follower.path[follower.nextStep]) - feet;
            if (glm::length(offset) > ArrivalDistance)
            {
                InputIntentions intentions;
                intentions.direction = glm::normalize(offset);
                return intentions;
            }
            ++follower.nextStep;
        }
        return {};
    }
}
