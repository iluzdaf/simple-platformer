#include "simple_platformer/movement/flying_movement.hpp"

#include <cmath>
#include <stdexcept>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/physics/collision.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    CollisionContacts updateFlyingMovement(
        const TileMap& map,
        Body& body,
        const FlyingMovement& movement,
        const InputIntentions& intentions,
        Facing& facing,
        float deltaTime)
    {
        if (!std::isfinite(deltaTime) || deltaTime <= 0.0F || !std::isfinite(movement.speed) ||
            movement.speed < 0.0F || !isFinite(intentions.direction))
        {
            throw std::invalid_argument("Flying movement data must be finite and non-negative");
        }

        glm::vec2 direction = intentions.direction;
        const float length = glm::length(direction);
        if (length > 1.0F)
        {
            direction /= length;
        }

        if (direction.x < 0.0F)
        {
            facing = Facing::Left;
        }
        else if (direction.x > 0.0F)
        {
            facing = Facing::Right;
        }

        body.velocity = direction * movement.speed;
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
