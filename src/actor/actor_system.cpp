#include "simple_platformer/actor/actor_system.hpp"

#include <stdexcept>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/input/input_state.hpp"
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
                updatePlatformerMovement(
                    map,
                    actor.body,
                    *actor.platformerMovement,
                    intentions,
                    actor.facing,
                    deltaTime);
            }
            else if (actor.flyingMovement.has_value())
            {
                updateFlyingMovement(
                    map, actor.body, *actor.flyingMovement, intentions, actor.facing, deltaTime);
            }
            else
            {
                throw std::logic_error("An actor has no movement component");
            }
        }
    }
}
