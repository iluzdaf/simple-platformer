#pragma once

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"

namespace simple_platformer
{
    class TileMap;

    struct Body
    {
        // Collision rectangle in world pixels. Its size is independent of any sprite.
        Aabb bounds;
        // Movement in world pixels per second.
        glm::vec2 velocity = {0.0F, 0.0F};
    };

    // What falls in this world falls at this rate unless its own configuration says otherwise.
    constexpr float DefaultGravity = 800.0F;
    constexpr float DefaultMaximumFallSpeed = 600.0F;

    void applyGravity(Body& body, float gravity, float maximumFallSpeed, float deltaTime);

    // The sides of a body that meet a tile surface or blocking map boundary.
    struct CollisionContacts
    {
        bool left = false;
        bool right = false;
        bool ground = false;
        bool ceiling = false;
    };

    // Moves the body with tile collision: X, then Y by velocity * deltaTime.
    // Blocking tiles stop travel and zero velocity on the axis that hits.
    CollisionContacts moveBody(const TileMap& map, Body& body, float deltaTime);

    // Probes for surfaces touching a stationary box without moving it,
    // allowing a small tolerance for floating-point positions.
    CollisionContacts touchingSurfaces(const TileMap& map, const Aabb& bounds);

    // Only marked solid tiles can hold a wall or ceiling climber.
    CollisionContacts touchingClimbableSurfaces(const TileMap& map, const Aabb& bounds);
}
