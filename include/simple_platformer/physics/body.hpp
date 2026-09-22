#pragma once

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/physics/collision.hpp"

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

    // Moves the body by its velocity over the step and stops it along any axis that hit a tile.
    CollisionContacts moveBody(const TileMap& map, Body& body, float deltaTime);
}
