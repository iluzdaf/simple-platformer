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

    // Where an actor is along the path it is following. An NPC actor keeps one; the NPC
    // system asks it for the intentions that move the actor, and the ordinary movement
    // systems do the moving.
    struct PathFollower
    {
        std::optional<NavigationPath> path;
        // The step being travelled; one past the last once the path is complete.
        std::size_t nextStep = 0;
        // How far into the current step's input program the follower is. Zero means the
        // program has not started, so the follower is still getting to its takeoff.
        float programElapsed = 0.0F;
        // The cell the path was requested for, so a request for the same cell again does
        // not search again.
        std::optional<GridPosition> destinationCell;
        // How long after a search before another may run, and how long of that is left.
        float repathCooldown = 0.25F;
        float repathRemaining = 0.0F;
    };

    // Starts following the path, which must end in the destination cell.
    void setPath(PathFollower& follower, NavigationPath path, GridPosition destinationCell);
    void clearPath(PathFollower& follower);
    // Whether every step has been travelled. Never true without a path.
    bool pathComplete(const PathFollower& follower);

    // The intentions that carry the actor towards its next step this tick. A flyer steers
    // straight at each step's cell. A platformer walks to a walk's cell and brakes there;
    // for a jump or a fall it first stops at the takeoff, then replays the recorded
    // inputs, and moves on once it stands in the step's cell.
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
