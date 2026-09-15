#pragma once

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"

namespace simple_platformer
{
    class TileMap;

    struct CollisionContacts
    {
        bool left = false;
        bool right = false;
        bool ground = false;
        bool ceiling = false;
    };

    CollisionContacts moveAndCollide(const TileMap& map, Aabb& bounds, glm::vec2 displacement);
}
