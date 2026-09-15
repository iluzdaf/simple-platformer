#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"

namespace simple_platformer
{
    struct Aabb;
    struct InputIntentions;

    struct PathFollower
    {
        std::vector<GridPosition> path;
        std::size_t nextStep = 0;
        std::optional<GridPosition> destination;
        float repathCooldown = 0.25F;
        float repathRemaining = 0.0F;
    };

    GridPosition navigationCell(glm::vec2 feet);
    glm::vec2 navigationFeet(GridPosition cell);
    void setPath(PathFollower& follower, std::vector<GridPosition> path, GridPosition destination);
    void clearPath(PathFollower& follower);
    bool pathComplete(const PathFollower& follower);
    InputIntentions followFlyingPath(const Aabb& bounds, PathFollower& follower);
}
