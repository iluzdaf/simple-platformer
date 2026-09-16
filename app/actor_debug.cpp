#include "actor_debug.hpp"

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/input_program.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/timing/fixed_step.hpp"
#include "simple_platformer/world/tile_map.hpp"
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

    std::vector<glm::vec2> sampleAirborneTraversal(
        const simple_platformer::Actor& actor,
        const simple_platformer::TileMap& map,
        simple_platformer::GridPosition start,
        const simple_platformer::NavigationStep& step)
    {
        if ((step.traversal != simple_platformer::Traversal::Jump &&
             step.traversal != simple_platformer::Traversal::Fall) ||
            step.inputs.empty() || !actor.platformerMovement.has_value())
        {
            return {};
        }

        constexpr float SimulationStep = static_cast<float>(simple_platformer::FixedDeltaSeconds);
        simple_platformer::Body body;
        body.bounds.size = actor.body.bounds.size;
        simple_platformer::placeFeetAt(body.bounds, simple_platformer::navigationFeet(start));
        simple_platformer::PlatformerMovement movement{
            actor.platformerMovement->config, true, 0.0F, 0.0F};
        simple_platformer::Facing facing = step.destination.x < start.x
                                               ? simple_platformer::Facing::Left
                                               : simple_platformer::Facing::Right;
        std::vector<glm::vec2> sampledFeet;
        sampledFeet.push_back(simple_platformer::feetOf(body.bounds));

        for (const simple_platformer::InputStep& input : step.inputs)
        {
            const long ticks = std::lround(input.duration / SimulationStep);
            for (long tick = 0; tick < ticks; ++tick)
            {
                simple_platformer::updatePlatformerMovement(
                    map, body, movement, input.intentions, facing, SimulationStep);
                sampledFeet.push_back(simple_platformer::feetOf(body.bounds));
            }
        }
        return sampledFeet;
    }

    simple_platformer::PathFollowerDebugInfo pathFollowerDebugInfo(
        const simple_platformer::Actor& actor,
        const simple_platformer::TileMap& map,
        const simple_platformer::PathFollower& follower)
    {
        simple_platformer::PathFollowerDebugInfo info;
        info.destination = follower.destination;
        info.repathRemaining = follower.repathRemaining;
        if (!follower.path.has_value())
        {
            return info;
        }

        info.hasPath = true;
        info.nextStep = follower.nextStep;
        info.stepCount = follower.path->steps.size();
        info.connections.reserve(info.stepCount);

        simple_platformer::GridPosition fromCell = follower.path->start;
        glm::vec2 from = simple_platformer::navigationFeet(fromCell);
        for (std::size_t index = 0; index < follower.path->steps.size(); ++index)
        {
            const simple_platformer::NavigationStep& step = follower.path->steps[index];
            const glm::vec2 to = simple_platformer::navigationFeet(step.destination);
            info.connections.push_back(
                {from,
                 to,
                 step.traversal,
                 index < follower.nextStep,
                 index == follower.nextStep,
                 sampleAirborneTraversal(actor, map, fromCell, step)});
            fromCell = step.destination;
            from = to;
        }
        return info;
    }

    simple_platformer::SensorDebugInfo sensorDebugInfo(
        const simple_platformer::Actor& actor,
        const simple_platformer::NpcBrain& brain,
        const simple_platformer::NpcSenses& senses,
        const simple_platformer::Actor* player)
    {
        simple_platformer::SensorDebugInfo info;
        info.observerCenter = simple_platformer::centerOf(actor.body.bounds);
        info.noticeDistance = senses.noticeDistance;
        info.memoryRemaining = brain.targetMemoryRemaining;

        if (brain.targetVisible && player != nullptr &&
            player->life == simple_platformer::LifeState::Alive &&
            simple_platformer::areOpponents(actor.team, player->team))
        {
            info.visibleTargetCenter = simple_platformer::centerOf(player->body.bounds);
        }
        else if (brain.target.has_value() && brain.targetMemoryRemaining > 0.0F)
        {
            info.rememberedTargetFeet = brain.lastSeenTargetFeet;
        }
        return info;
    }
}

namespace simple_platformer
{
    ActorDebugScene makeActorDebugScene(
        const World& world,
        const TileMap& map,
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
        const Actor* player = world.findActor(world.playerId());

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
            if (actor.pathFollower.has_value())
            {
                info.pathFollower = pathFollowerDebugInfo(actor, map, actor.pathFollower.value());
            }
            if (actor.brain.has_value() && actor.senses.has_value())
            {
                info.sensor =
                    sensorDebugInfo(actor, actor.brain.value(), actor.senses.value(), player);
            }
            scene.actors.push_back(info);
        }

        return scene;
    }
}
