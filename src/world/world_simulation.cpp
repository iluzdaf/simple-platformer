#include "simple_platformer/world/world_simulation.hpp"

#include "simple_platformer/actor/actor_system.hpp"
#include "simple_platformer/actor/lifecycle.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/combat/projectile_system.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/navigation/navigation_fill.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/npc/npc_system.hpp"
#include "simple_platformer/timing/frame_profile.hpp"
#include "simple_platformer/world/level_exit.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"

namespace simple_platformer
{
    void updateWorldSimulation(TileMap& map, World& world, float deltaTime, FrameProfile* profile)
    {
        if (world.levelComplete())
        {
            return;
        }
        world.advanceSimulationTime(deltaTime);
        holdPlayerAtOpeningExit(world);
        // Each phase is charged to the profile under its category, when there is one.
        const auto phase = [&](const char* category, const char* name, auto&& run)
        { timePhase(profile, category, name, run); };

        phase(
            "NPC",
            "Navigation fill",
            [&]
            {
                const FillWork work =
                    fillNavigation(map, world.platformerConnections(), NavigationFillTicksPerStep);
                if (profile != nullptr)
                {
                    profile->navigationFillTicks += work.simulatedTicks;
                }
            });
        phase("NPC", "NPC senses", [&] { updateNpcSenses(map, world, deltaTime); });
        phase("NPC", "NPC behaviour", [&] { updateNpcBehaviour(map, world, deltaTime, profile); });
        phase("Movement", "Actor movement", [&] { updateActorMovement(map, world, deltaTime); });
        phase("Movement", "Pickup movement", [&] { updatePickupMovement(map, world, deltaTime); });
        WorldRequests requests;
        phase("Combat", "Attacks", [&] { updateAttacks(world, requests, deltaTime); });
        phase("Combat", "Projectiles", [&] { updateProjectiles(map, world, requests, deltaTime); });
        phase(
            "Combat",
            "Projectile bursts",
            [&] { updateProjectileBursts(world, requests, deltaTime); });
        phase("World", "Life states", [&] { updateLifeState(world, requests, deltaTime); });
        phase("World", "Pickups", [&] { updatePickups(world, requests); });
        phase("World", "World requests", [&] { applyWorldRequests(world, requests); });
        phase("World", "Level exit", [&] { updateLevelExit(world); });
    }
}
