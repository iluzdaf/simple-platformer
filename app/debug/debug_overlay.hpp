#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/npc/npc_state_machine.hpp"

#include "debug/navigation_debug.hpp"

namespace simple_platformer
{
    class World;
    class TileMap;
    struct CameraController;
    enum class AnimationName;
    enum class NpcState;
    enum class NpcTactic;

    enum class ActorDebugKind
    {
        Player,
        Npc,
        Actor
    };

    struct ActorSpriteDebugInfo
    {
        Aabb bounds;
        std::size_t atlasFrame = 0;
        glm::vec2 atlasPosition = {0.0F, 0.0F};
    };

    struct PathConnectionDebugInfo
    {
        glm::vec2 fromFeet = {0.0F, 0.0F};
        glm::vec2 toFeet = {0.0F, 0.0F};
        Traversal traversal = Traversal::Fly;
        bool completed = false;
        bool next = false;
        std::vector<glm::vec2> sampledFeet;
    };

    struct PathFollowerDebugInfo
    {
        bool hasPath = false;
        std::size_t nextStep = 0;
        std::size_t stepCount = 0;
        // Resolved to feet here, like the connections, so the UI draws without cell maths.
        std::optional<glm::vec2> destinationFeet;
        std::vector<PathConnectionDebugInfo> connections;
    };

    struct SensorDebugInfo
    {
        glm::vec2 observerCenter = {0.0F, 0.0F};
        float noticeDistance = 0.0F;
        std::optional<glm::vec2> visibleTargetCenter;
        std::optional<glm::vec2> rememberedTargetFeet;
        float memoryRemaining = 0.0F;
    };

    struct PatrolDebugInfo
    {
        glm::vec2 firstFeet = {0.0F, 0.0F};
        glm::vec2 secondFeet = {0.0F, 0.0F};
        bool headingToSecond = true;
    };

    struct ActorDebugInfo
    {
        ActorId id;
        ActorDebugKind kind = ActorDebugKind::Actor;
        Aabb collider;
        std::optional<ActorSpriteDebugInfo> sprite;
        std::optional<AnimationName> animation;
        std::optional<NpcState> npcState;
        // Whichever decides the NPC's state: its tactic or its machine.
        std::optional<NpcTactic> npcTactic;
        std::optional<std::string> machine;
        std::optional<PathFollowerDebugInfo> pathFollower;
        std::optional<SensorDebugInfo> sensor;
        std::optional<PatrolDebugInfo> patrol;
        std::optional<Aabb> biteHitbox;
    };

    struct ProjectileDebugInfo
    {
        Aabb bounds;
        float lifetimeRemaining = 0.0F;
        std::optional<ActorId> owner;
    };

    struct PickupDebugInfo
    {
        Aabb bounds;
        std::string itemName;
    };

    // The machine of the NPC the machine window follows: its definition to draw, which
    // state is active and which transition fired last.
    struct MachineDebugInfo
    {
        ActorId actor;
        NpcStateMachine definition;
        std::size_t active = 0;
        std::optional<std::size_t> lastFired;
    };

    // What the overlay shows of the world: only what the camera can see, a tile beyond
    // its edges, so a large level does not fill the text column with actors off screen.
    struct DebugOverlay
    {
        std::vector<ActorDebugInfo> actors;
        std::vector<ProjectileDebugInfo> projectiles;
        std::vector<PickupDebugInfo> pickups;
        std::optional<NavigationCacheDebugInfo> navigationCache;
        // The cell under the cursor when its tile can break, for the hint that B breaks it.
        std::optional<Aabb> breakableCellUnderCursor;
        // The machine of the NPC under the cursor, or else of the NPC with a machine
        // nearest the player; absent while no NPC on screen has one.
        std::optional<MachineDebugInfo> machine;
        Aabb cameraBounds;
        Aabb cameraDeadZone;
    };

    // simulationStepSeconds is the fixed step the world is simulated with; predicted jump
    // arcs are replayed at it so they match what the actor will do. The navigation view
    // says which cell and which body the cache is shown for, and its cursor also picks
    // the NPC whose machine is shown.
    DebugOverlay makeDebugOverlay(
        const World& world,
        const TileMap& map,
        const CameraController& cameraController,
        float atlasWidth,
        float simulationStepSeconds,
        const NavigationDebugView& navigation = {});
}
