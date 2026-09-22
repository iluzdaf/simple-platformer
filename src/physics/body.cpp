#include "simple_platformer/physics/body.hpp"

#include <algorithm>

#include "simple_platformer/physics/collision.hpp"

namespace simple_platformer
{
    void applyGravity(Body& body, float gravity, float maximumFallSpeed, float deltaTime)
    {
        body.velocity.y = std::min(body.velocity.y + gravity * deltaTime, maximumFallSpeed);
    }

    CollisionContacts moveBody(const TileMap& map, Body& body, float deltaTime)
    {
        const CollisionContacts contacts =
            moveAndCollide(map, body.bounds, body.velocity * deltaTime);
        if (contacts.left || contacts.right)
        {
            body.velocity.x = 0.0F;
        }
        if (contacts.ground || contacts.ceiling)
        {
            body.velocity.y = 0.0F;
        }
        return contacts;
    }
}
