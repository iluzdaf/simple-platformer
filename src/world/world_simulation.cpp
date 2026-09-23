#include "simple_platformer/world/world_simulation.hpp"

#include <chrono>
#include <functional>

#include "simple_platformer/actor/actor_system.hpp"
#include "simple_platformer/actor/lifecycle.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/combat/projectile_system.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/npc/npc_system.hpp"
#include "simple_platformer/timing/frame_profile.hpp"
#include "simple_platformer/world/level_exit.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"

namespace simple_platformer
{
    namespace
    {
        // Runs one phase, and charges its wall-clock time to the profile when there is one.
        void timePhase(FrameProfile* profile, const char* name, const std::function<void()>& phase)
        {
            if (profile == nullptr)
            {
                phase();
                return;
            }
            const auto start = std::chrono::steady_clock::now();
            phase();
            addPhaseSeconds(
                *profile,
                name,
                std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count());
        }
    }

    void updateWorldSimulation(TileMap& map, World& world, float deltaTime, FrameProfile* profile)
    {
        if (world.levelComplete())
        {
            return;
        }
        world.advanceSimulationTime(deltaTime);
        holdPlayerAtOpeningExit(world);
        timePhase(profile, "NPC senses", [&] { updateNpcSenses(map, world, deltaTime); });
        timePhase(
            profile, "NPC behaviour", [&] { updateNpcBehaviour(map, world, deltaTime, profile); });
        timePhase(profile, "Actor movement", [&] { updateActorMovement(map, world, deltaTime); });
        timePhase(profile, "Pickup movement", [&] { updatePickupMovement(map, world, deltaTime); });
        WorldRequests requests;
        timePhase(profile, "Attacks", [&] { updateAttacks(world, requests, deltaTime); });
        timePhase(
            profile, "Projectiles", [&] { updateProjectiles(map, world, requests, deltaTime); });
        timePhase(
            profile,
            "Projectile bursts",
            [&] { updateProjectileBursts(world, requests, deltaTime); });
        timePhase(profile, "Life states", [&] { updateLifeState(world, requests, deltaTime); });
        timePhase(profile, "Pickups", [&] { updatePickups(world, requests); });
        timePhase(profile, "World requests", [&] { applyWorldRequests(world, requests); });
        timePhase(profile, "Level exit", [&] { updateLevelExit(world); });
    }
}
