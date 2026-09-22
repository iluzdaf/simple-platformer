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
        float deltaTime)
    {
        requireTimeStep(deltaTime, "Flying movement");
        if (!std::isfinite(movement.speed) || movement.speed < 0.0F ||
            !isFinite(intentions.direction))
        {
            throw std::invalid_argument(
                "Flying movement requires a finite, non-negative speed and finite intentions");
        }

        glm::vec2 direction = intentions.direction;
        const float length = glm::length(direction);
        if (length > 1.0F)
        {
            direction /= length;
        }

        body.velocity = direction * movement.speed;
        return moveBody(map, body, deltaTime);
    }
}
