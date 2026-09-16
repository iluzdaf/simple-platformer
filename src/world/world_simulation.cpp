#include "simple_platformer/world/world_simulation.hpp"

#include "simple_platformer/actor/actor_system.hpp"
#include "simple_platformer/actor/lifecycle.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/combat/projectile_system.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/npc/npc_system.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"

namespace simple_platformer
{
    void updateWorldSimulation(const TileMap& map, World& world, float deltaTime)
    {
        WorldRequests requests;
        updateNpcSenses(map, world, deltaTime);
        updateNpcBehaviour(map, world, deltaTime);
        updateActorMovement(map, world, deltaTime);
        updateAttacks(world, requests, deltaTime);
        updateProjectiles(map, world, requests, deltaTime);
        updateLifeState(world, requests, deltaTime);
        applyWorldRequests(world, requests);
    }
}
