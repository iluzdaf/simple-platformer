#include "debug_overlay.hpp"

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/attack_system.hpp"
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
#include "simple_platformer/world/pickup.hpp"

namespace simple_platformer
{
    namespace
    {
        ActorDebugKind kindOf(const Actor& actor, ActorId playerId)
        {
            if (actor.id == playerId)
            {
                return ActorDebugKind::Player;
            }
            if (actor.brain.has_value())
            {
                return ActorDebugKind::Npc;
            }
            return ActorDebugKind::Actor;
        }

        ActorSpriteDebugInfo spriteDebugInfo(
            const Actor& actor,
            const Sprite& sprite,
            float atlasWidth)
        {
            if (!isFinite(sprite.region.position) || !isFinite(sprite.region.size) ||
                !isFinite(sprite.size) || sprite.region.position.x < 0.0F ||
                sprite.region.position.y < 0.0F || sprite.region.size.x <= 0.0F ||
                sprite.region.size.y <= 0.0F || sprite.size.x <= 0.0F || sprite.size.y <= 0.0F ||
                atlasWidth < sprite.region.size.x)
            {
                throw std::logic_error("Debug overlay requires a valid sprite region");
            }

            const Aabb bounds = spriteBounds(actor.body.bounds, sprite);
            const std::size_t atlasColumns =
                static_cast<std::size_t>(atlasWidth / sprite.region.size.x);
            const std::size_t atlasColumn =
                static_cast<std::size_t>(sprite.region.position.x / sprite.region.size.x);
            const std::size_t atlasRow =
                static_cast<std::size_t>(sprite.region.position.y / sprite.region.size.y);
            return {bounds, atlasRow * atlasColumns + atlasColumn + 1, sprite.region.position};
        }

        std::vector<glm::vec2> sampleAirborneTraversal(
            const Actor& actor,
            const TileMap& map,
            GridPosition start,
            const NavigationStep& step)
        {
            if ((step.traversal != Traversal::Jump && step.traversal != Traversal::Fall) ||
                step.inputs.empty() || !actor.platformerMovement.has_value())
            {
                return {};
            }

            constexpr float SimulationStep = static_cast<float>(FixedDeltaSeconds);
            Body body;
            body.bounds = boxInCell(map.tileSize(), start, actor.body.bounds.size);
            PlatformerMovement movement{actor.platformerMovement->config, true, 0.0F, 0.0F};
            Facing facing = step.destination.x < start.x ? Facing::Left : Facing::Right;
            std::vector<glm::vec2> sampledFeet;
            sampledFeet.push_back(feetOf(body.bounds));

            for (const InputStep& input : step.inputs)
            {
                const long ticks = std::lround(input.duration / SimulationStep);
                for (long tick = 0; tick < ticks; ++tick)
                {
                    updatePlatformerMovement(
                        map, body, movement, input.intentions, facing, SimulationStep);
                    sampledFeet.push_back(feetOf(body.bounds));
                }
            }
            return sampledFeet;
        }

        PathFollowerDebugInfo pathFollowerDebugInfo(
            const Actor& actor,
            const TileMap& map,
            const PathFollower& follower)
        {
            PathFollowerDebugInfo info;
            if (follower.destination.has_value())
            {
                info.destinationFeet = feetInCell(map.tileSize(), *follower.destination);
            }
            info.repathRemaining = follower.repathRemaining;
            if (!follower.path.has_value())
            {
                return info;
            }

            info.hasPath = true;
            info.nextStep = follower.nextStep;
            info.stepCount = follower.path->steps.size();
            info.connections.reserve(info.stepCount);

            GridPosition fromCell = follower.path->start;
            glm::vec2 from = feetInCell(map.tileSize(), fromCell);
            for (std::size_t index = 0; index < follower.path->steps.size(); ++index)
            {
                const NavigationStep& step = follower.path->steps[index];
                const glm::vec2 to = feetInCell(map.tileSize(), step.destination);
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

        SensorDebugInfo sensorDebugInfo(
            const Actor& actor,
            const NpcBrain& brain,
            const NpcSenses& senses,
            const Actor* player)
        {
            SensorDebugInfo info;
            info.observerCenter = centerOf(actor.body.bounds);
            info.noticeDistance = senses.noticeDistance;
            info.memoryRemaining = brain.targetMemoryRemaining;

            if (brain.targetVisible && player != nullptr && player->life == LifeState::Alive &&
                areOpponents(actor.team, player->team))
            {
                info.visibleTargetCenter = centerOf(player->body.bounds);
            }
            else if (brain.target.has_value() && brain.targetMemoryRemaining > 0.0F)
            {
                info.rememberedTargetFeet = brain.lastSeenTargetFeet;
            }
            return info;
        }
    }

    DebugOverlay makeDebugOverlay(
        const World& world,
        const TileMap& map,
        const CameraController& cameraController,
        float atlasWidth)
    {
        if (!std::isfinite(atlasWidth) || atlasWidth <= 0.0F)
        {
            throw std::invalid_argument("Debug overlay atlas width must be positive and finite");
        }

        DebugOverlay scene;
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
            if (actor.patrol.has_value())
            {
                const Patrol& patrol = actor.patrol.value();
                info.patrol =
                    PatrolDebugInfo{patrol.firstFeet, patrol.secondFeet, patrol.headingToSecond};
            }
            if (actor.bite.has_value() && actor.bite->phase == BitePhase::Active)
            {
                info.biteHitbox = biteHitbox(actor.body.bounds, actor.bite.value(), actor.facing);
            }
            scene.actors.push_back(info);
        }

        scene.projectiles.reserve(world.projectiles().size());
        for (const Projectile& projectile : world.projectiles())
        {
            scene.projectiles.push_back(
                {projectile.bounds, projectile.remainingLifetime, projectile.owner});
        }

        scene.pickups.reserve(world.pickups().size());
        for (const Pickup& pickup : world.pickups())
        {
            scene.pickups.push_back(
                {pickup.body.bounds, world.itemDefinition(pickup.stack.item).name});
        }

        return scene;
    }
}
