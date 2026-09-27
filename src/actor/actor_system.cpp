#include "simple_platformer/actor/actor_system.hpp"

#include <stdexcept>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    void updateActorMovement(const TileMap& map, World& world, float deltaTime)
    {
        for (Actor& actor : world.actors())
        {
            const InputIntentions intentions =
                actor.life == LifeState::Alive ? actor.intentions : InputIntentions{};
            if (actor.platformerMovement.has_value())
            {
                PlatformerMovement& movement = *actor.platformerMovement;
                const bool wasGrounded = movement.grounded;
                updatePlatformerMovement(map, actor.body, movement, intentions, deltaTime);
                if (!wasGrounded && movement.grounded && actor.life == LifeState::Alive)
                {
                    world.emitNoise({actor.id, feetOf(actor.body.bounds), NoiseKind::Landing});
                }
            }
            else if (actor.flyingMovement.has_value())
            {
                updateFlyingMovement(map, actor.body, *actor.flyingMovement, intentions, deltaTime);
            }
            else
            {
                throw std::logic_error("An actor has no movement component");
            }

            actor.facing = facingFor(intentions, actor.facing);
        }
    }
}
