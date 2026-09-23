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
        void timePhase(
            FrameProfile* profile,
            const char* category,
            const char* name,
            const std::function<void()>& phase)
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
                category,
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
        timePhase(profile, "NPC", "NPC senses", [&] { updateNpcSenses(map, world, deltaTime); });
        timePhase(
            profile,
            "NPC",
            "NPC behaviour",
            [&] { updateNpcBehaviour(map, world, deltaTime, profile); });
        timePhase(
            profile,
            "Movement",
            "Actor movement",
            [&] { updateActorMovement(map, world, deltaTime); });
        timePhase(
            profile,
            "Movement",
            "Pickup movement",
            [&] { updatePickupMovement(map, world, deltaTime); });
        WorldRequests requests;
        timePhase(profile, "Combat", "Attacks", [&] { updateAttacks(world, requests, deltaTime); });
        timePhase(
            profile,
            "Combat",
            "Projectiles",
            [&] { updateProjectiles(map, world, requests, deltaTime); });
        timePhase(
            profile,
            "Combat",
            "Projectile bursts",
            [&] { updateProjectileBursts(world, requests, deltaTime); });
        timePhase(
            profile, "World", "Life states", [&] { updateLifeState(world, requests, deltaTime); });
        timePhase(profile, "World", "Pickups", [&] { updatePickups(world, requests); });
        timePhase(profile, "World", "World requests", [&] { applyWorldRequests(world, requests); });
        timePhase(profile, "World", "Level exit", [&] { updateLevelExit(world); });
    }
}
