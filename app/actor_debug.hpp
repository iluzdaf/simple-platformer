#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

namespace simple_platformer
{
    class World;
    class TileMap;
    struct CameraController;
    enum class AnimationName;
    enum class NpcState;

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
        std::optional<GridPosition> destination;
        float repathRemaining = 0.0F;
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

    struct ActorDebugInfo
    {
        ActorId id;
        ActorDebugKind kind = ActorDebugKind::Actor;
        Aabb collider;
        std::optional<ActorSpriteDebugInfo> sprite;
        std::optional<AnimationName> animation;
        std::optional<NpcState> npcState;
        std::optional<PathFollowerDebugInfo> pathFollower;
        std::optional<SensorDebugInfo> sensor;
    };

    struct ProjectileDebugInfo
    {
        Aabb bounds;
        float remainingLifetime = 0.0F;
        std::optional<ActorId> owner;
    };

    struct ActorDebugScene
    {
        std::vector<ActorDebugInfo> actors;
        std::vector<ProjectileDebugInfo> projectiles;
        Aabb cameraBounds;
        Aabb cameraDeadZone;
    };

    ActorDebugScene makeActorDebugScene(
        const World& world,
        const TileMap& map,
        const CameraController& cameraController,
        float atlasWidth);
}
