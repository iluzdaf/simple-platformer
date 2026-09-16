#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/math/aabb.hpp"

namespace simple_platformer
{
    class World;
    struct Camera;
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

    struct ActorDebugInfo
    {
        ActorId id;
        ActorDebugKind kind = ActorDebugKind::Actor;
        Aabb collider;
        std::optional<ActorSpriteDebugInfo> sprite;
        std::optional<AnimationName> animation;
        std::optional<NpcState> npcState;
    };

    struct ActorDebugScene
    {
        std::vector<ActorDebugInfo> actors;
        glm::vec2 cameraPosition = {0.0F, 0.0F};
    };

    ActorDebugScene makeActorDebugScene(const World& world, const Camera& camera, float atlasWidth);
}
