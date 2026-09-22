#include "simple_platformer/world/world_simulation.hpp"

#include "simple_platformer/actor/actor_system.hpp"
#include "simple_platformer/actor/lifecycle.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/combat/projectile_system.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/npc/npc_system.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/level_exit.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"

namespace simple_platformer
{
    void updateWorldSimulation(TileMap& map, World& world, float deltaTime)
    {
        if (world.levelComplete())
        {
            return;
        }
        world.advanceSimulationTime(deltaTime);
        WorldRequests requests;
        updateNpcSenses(map, world, deltaTime);
        updateNpcBehaviour(map, world, deltaTime);
        updateActorMovement(map, world, deltaTime);
        updatePickupMovement(map, world, deltaTime);
        updateAttacks(world, requests, deltaTime);
        updateProjectiles(map, world, requests, deltaTime);
        updateProjectileBursts(world, requests, deltaTime);
        updateLifeState(world, requests, deltaTime);
        updatePickups(world, requests);
        applyWorldRequests(world, requests);
        updateLevelExit(world);
    }
}
