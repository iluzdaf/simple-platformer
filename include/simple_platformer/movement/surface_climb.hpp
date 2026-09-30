#pragma once

#include "simple_platformer/physics/collision.hpp"

namespace simple_platformer
{
    class TileMap;
    struct Body;
    struct InputIntentions;
    struct PlatformerMovement;

    enum class ClimbSurface
    {
        None,
        LeftWall,
        RightWall,
        Ceiling
    };

    // Which way a climber's head points along a wall.
    enum class WallHeading
    {
        Up,
        Down
    };

    struct SurfaceClimbConfig
    {
        float speed = 60.0F;
    };

    // Optional capability for a platformer actor. The surface is runtime state;
    // climbRequested in the current intentions decides whether it stays attached.
    struct SurfaceClimb
    {
        SurfaceClimbConfig config;
        ClimbSurface surface = ClimbSurface::None;
        // Kept while the climber holds still on a wall, so it does not turn round.
        WallHeading wallHeading = WallHeading::Up;
    };

    void validateSurfaceClimbConfig(const SurfaceClimbConfig& config);

    // On a wall, the way the intentions climb, or the current heading when they hold
    // still. Off a wall, Up, which is where a climber heads on the next wall until it moves.
    WallHeading wallHeadingFor(
        ClimbSurface surface,
        const InputIntentions& intentions,
        WallHeading current);

    // Whether the contacts include the wall or ceiling. The floor is not a climb surface.
    bool touchesSurface(ClimbSurface surface, const CollisionContacts& contacts);

    CollisionContacts updateSurfaceClimbMovement(
        const TileMap& map,
        Body& body,
        PlatformerMovement& movement,
        SurfaceClimb& climb,
        const InputIntentions& intentions,
        float deltaTime);
}
