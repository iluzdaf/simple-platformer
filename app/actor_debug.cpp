#include "actor_debug.hpp"

#include <cmath>
#include <cstddef>
#include <stdexcept>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/world.hpp"

namespace
{
    simple_platformer::ActorDebugKind kindOf(
        const simple_platformer::Actor& actor,
        simple_platformer::ActorId playerId)
    {
        if (actor.id == playerId)
        {
            return simple_platformer::ActorDebugKind::Player;
        }
        if (actor.brain.has_value())
        {
            return simple_platformer::ActorDebugKind::Npc;
        }
        return simple_platformer::ActorDebugKind::Actor;
    }

    simple_platformer::ActorSpriteDebugInfo spriteDebugInfo(
        const simple_platformer::Actor& actor,
        const simple_platformer::Sprite& sprite,
        float atlasWidth)
    {
        if (!simple_platformer::isFinite(sprite.region.position) ||
            !simple_platformer::isFinite(sprite.region.size) ||
            !simple_platformer::isFinite(sprite.size) || sprite.region.position.x < 0.0F ||
            sprite.region.position.y < 0.0F || sprite.region.size.x <= 0.0F ||
            sprite.region.size.y <= 0.0F || sprite.size.x <= 0.0F || sprite.size.y <= 0.0F ||
            atlasWidth < sprite.region.size.x)
        {
            throw std::logic_error("Actor debug requires a valid sprite region");
        }

        const simple_platformer::Aabb bounds =
            simple_platformer::spriteBounds(actor.body.bounds, sprite);
        const std::size_t atlasColumns =
            static_cast<std::size_t>(atlasWidth / sprite.region.size.x);
        const std::size_t atlasColumn =
            static_cast<std::size_t>(sprite.region.position.x / sprite.region.size.x);
        const std::size_t atlasRow =
            static_cast<std::size_t>(sprite.region.position.y / sprite.region.size.y);
        return {bounds, atlasRow * atlasColumns + atlasColumn + 1, sprite.region.position};
    }
}

namespace simple_platformer
{
    ActorDebugScene makeActorDebugScene(
        const World& world,
        const CameraController& cameraController,
        float atlasWidth)
    {
        if (!std::isfinite(atlasWidth) || atlasWidth <= 0.0F)
        {
            throw std::invalid_argument("Actor debug atlas width must be positive and finite");
        }

        ActorDebugScene scene;
        scene.cameraBounds = {
            cameraController.camera.position, cameraController.camera.viewportSize};
        scene.cameraDeadZone = {
            cameraController.camera.position +
                (cameraController.camera.viewportSize - cameraController.deadZoneSize) * 0.5F,
            cameraController.deadZoneSize};
        scene.actors.reserve(world.actors().size());

        for (const Actor& actor : world.actors())
        {
            ActorDebugInfo info;
            info.id = actor.id;
            info.kind = kindOf(actor, world.playerId());
            info.collider = actor.body.bounds;
            if (actor.sprite.has_value())
            {
                info.sprite = spriteDebugInfo(actor, actor.sprite.value(), atlasWidth);
            }
            if (actor.animator.has_value())
            {
                info.animation = actor.animator->current;
            }
            if (actor.brain.has_value())
            {
                info.npcState = actor.brain->state;
            }
            scene.actors.push_back(info);
        }

        return scene;
    }
}
