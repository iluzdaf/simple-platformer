#pragma once

#include <cstddef>
#include <optional>

#include <glm/vec2.hpp>

#include "simple_platformer/navigation/navigation_path.hpp"

namespace simple_platformer
{
    struct Aabb;
    struct Body;
    struct FlyingMovement;
    struct InputIntentions;
    struct PlatformerMovement;
    struct SurfaceClimb;

    // Where an actor is along the path it is following. An NPC actor keeps one; the NPC
    // system asks it for the intentions that move the actor, and the ordinary movement
    // systems do the moving.
    struct PathFollower
    {
        std::optional<NavigationPath> path;
        // The waypoint being travelled to; one past the last once the path is complete.
        std::size_t nextStep = 0;
        // How far into the current step's input program the follower is. Zero means the
        // program has not started, so the follower is still getting to its takeoff.
        float programElapsed = 0.0F;
        // The point the path was requested for, so a request for much the same point
        // again does not search again. The path ends short of it when it cannot be
        // reached.
        std::optional<glm::vec2> target;
        // How many tiles the map had broken when the path was planned, so a break after
        // that, which the path may run through, has it planned again.
        std::size_t breaksWhenPlanned = 0;
    };

    // Starts following the path from its first waypoint.
    void setPath(PathFollower& follower, NavigationPath path);
    void clearPath(PathFollower& follower);
    // Whether every waypoint has been reached. Never true without a path.
    bool pathComplete(const PathFollower& follower);

    // The intentions that carry a flyer towards its next waypoint this tick. It steers
    // straight at each waypoint and slows on the last tick so it stops on it.
    InputIntentions followFlyingPath(
        const Aabb& bounds,
        const FlyingMovement& movement,
        PathFollower& follower,
        float deltaTime);

    // The intentions that carry a platformer towards its next waypoint this tick.
    // - Walk: walks to the waypoint and brakes to a stop there.
    // - Jump or fall: stops at the takeoff, replays the recorded inputs, and is done once
    //   it lands and stops on the waypoint's row.
    // - Climb: reaches the climb's start, along the surface if it is holding one, then
    //   replays the inputs. Needs the actor's climb; without it a climb step throws.
    // A step that ends away from its waypoint drops the path. Between steps the grip is
    // left alone, so a climber stays on what it holds.
    InputIntentions followPlatformerPath(
        const Body& body,
        const PlatformerMovement& movement,
        PathFollower& follower,
        float deltaTime,
        const SurfaceClimb* climb = nullptr);
}
