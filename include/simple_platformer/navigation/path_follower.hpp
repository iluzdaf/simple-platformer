#pragma once

#include <cstddef>
#include <optional>

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

namespace simple_platformer
{
    struct Aabb;
    struct Body;
    struct FlyingMovement;
    struct InputIntentions;
    struct PlatformerMovement;

    struct PathFollower
    {
        std::optional<NavigationPath> path;
        std::size_t nextStep = 0;
        float programElapsed = 0.0F;
        std::optional<GridPosition> destinationCell;
        float repathCooldown = 0.25F;
        float repathRemaining = 0.0F;
    };

    void setPath(PathFollower& follower, NavigationPath path, GridPosition destinationCell);
    void clearPath(PathFollower& follower);
    bool pathComplete(const PathFollower& follower);
    InputIntentions followFlyingPath(
        int tileSize,
        const Aabb& bounds,
        const FlyingMovement& movement,
        PathFollower& follower,
        float deltaTime);
    InputIntentions followPlatformerPath(
        int tileSize,
        const Body& body,
        const PlatformerMovement& movement,
        PathFollower& follower,
        float deltaTime);
}
