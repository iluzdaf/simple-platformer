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
    };

    void validateSurfaceClimbConfig(const SurfaceClimbConfig& config);

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
