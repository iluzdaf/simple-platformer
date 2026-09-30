#include "debug_overlay.hpp"

#include "navigation_debug.hpp"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_state_machine.hpp"
#include "simple_platformer/render/actor_sprite.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/render/sprite.hpp"
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
            if (!isFiniteNonNegative(sprite.region.position) ||
                !isFinitePositive(sprite.region.size) || !isFinitePositive(sprite.size) ||
                atlasWidth < sprite.region.size.x)
            {
                throw std::logic_error("Debug overlay requires a valid sprite region");
            }

            const Aabb bounds = placeActorSprite(actor).visible;
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
            glm::vec2 startFeet,
            const Waypoint& waypoint,
            float stepSeconds)
        {
            if (!actor.platformerMovement.has_value())
            {
                return {};
            }
            return sampleAirborneProgram(
                map,
                startFeet,
                actor.body.bounds.size,
                actor.platformerMovement.value().config,
                waypoint.traversal,
                waypoint.inputs,
                stepSeconds);
        }

        PathFollowerDebugInfo pathFollowerDebugInfo(
            const Actor& actor,
            const TileMap& map,
            const PathFollower& follower,
            float stepSeconds)
        {
            PathFollowerDebugInfo info;
            info.destinationFeet = follower.target;
            if (!follower.path.has_value())
            {
                return info;
            }

            info.hasPath = true;
            info.nextStep = follower.nextStep;
            info.stepCount = follower.path->waypoints.size();
            info.connections.reserve(info.stepCount);

            glm::vec2 from = follower.path->startFeet;
            for (std::size_t index = 0; index < follower.path->waypoints.size(); ++index)
            {
                const Waypoint& waypoint = follower.path->waypoints[index];
                info.connections.push_back(
                    {from,
                     waypoint.feet,
                     waypoint.traversal,
                     index < follower.nextStep,
                     index == follower.nextStep,
                     sampleAirborneTraversal(actor, map, from, waypoint, stepSeconds)});
                from = waypoint.feet;
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

            if (actor.perception.has_value() && actor.perception->targetVisible &&
                player != nullptr && player->life == LifeState::Alive &&
                areOpponents(actor.team, player->team))
            {
                info.visibleTargetCenter = centerOf(player->body.bounds);
            }
            else if (brain.target.has_value() && brain.targetMemoryRemaining > 0.0F)
            {
                info.rememberedTargetFeet = brain.lastKnownTargetFeet;
            }
            return info;
        }

        // The actors the overlay shows: those the camera can see, a tile beyond its edges.
        std::vector<const Actor*> actorsInView(const World& world, const Aabb& view)
        {
            std::vector<const Actor*> shown;
            for (const Actor& actor : world.actors())
            {
                if (overlaps(actor.body.bounds, view))
                {
                    shown.push_back(&actor);
                }
            }
            return shown;
        }

        const Actor* machineNpcUnderCursor(
            const std::vector<const Actor*>& shown,
            std::optional<glm::vec2> cursorWorld)
        {
            if (!cursorWorld.has_value())
            {
                return nullptr;
            }
            for (const Actor* actor : shown)
            {
                if (actor->machine.has_value() &&
                    contains(actor->body.bounds, cursorWorld.value_or(glm::vec2{})))
                {
                    return actor;
                }
            }
            return nullptr;
        }

        const Actor* nearestNpcWithMachine(
            const std::vector<const Actor*>& shown,
            const Aabb& nearTo)
        {
            const Actor* nearest = nullptr;
            for (const Actor* actor : shown)
            {
                if (!actor->machine.has_value())
                {
                    continue;
                }
                const glm::vec2 target = centerOf(nearTo);
                if (nearest == nullptr || glm::distance(centerOf(actor->body.bounds), target) <
                                              glm::distance(centerOf(nearest->body.bounds), target))
                {
                    nearest = actor;
                }
            }
            return nearest;
        }

        // The bounds of the cell under the cursor when its tile can break, for the hint
        // that B breaks it; nothing off the map or over a tile that cannot.
        std::optional<Aabb> breakableCellUnderCursor(
            const TileMap& map,
            std::optional<glm::vec2> cursorWorld)
        {
            if (!cursorWorld.has_value())
            {
                return std::nullopt;
            }
            const glm::vec2 cursor = cursorWorld.value_or(glm::vec2{0.0F, 0.0F});
            if (cursor.x < 0.0F || cursor.y < 0.0F || cursor.x >= map.pixelWidth() ||
                cursor.y >= map.pixelHeight())
            {
                return std::nullopt;
            }
            const Cell cell = cellAt(map.tileSize(), cursor);
            if (!map.definitionAt(cell).breaksIntoTileId.has_value())
            {
                return std::nullopt;
            }
            const auto tileSize = static_cast<float>(map.tileSize());
            return Aabb{
                {static_cast<float>(cell.x) * tileSize, static_cast<float>(cell.y) * tileSize},
                {tileSize, tileSize}};
        }
    }

    DebugOverlay makeDebugOverlay(
        const World& world,
        const TileMap& map,
        const CameraController& cameraController,
        float atlasWidth,
        float simulationStepSeconds,
        const NavigationDebugView& navigation,
        std::optional<ActorId> lockedMachineActor)
    {
        if (!isFinitePositive(simulationStepSeconds))
        {
            throw std::invalid_argument(
                "Debug overlay simulation step must be finite and positive");
        }
        if (!isFinitePositive(atlasWidth))
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
        const auto margin = static_cast<float>(map.tileSize());
        const Aabb view{
            scene.cameraBounds.topLeft - glm::vec2{margin, margin},
            scene.cameraBounds.size + glm::vec2{margin, margin} * 2.0F};
        const std::vector<const Actor*> shown = actorsInView(world, view);
        scene.actors.reserve(shown.size());
        const Actor* player = world.findActor(world.playerId());

        for (const Actor* shownActor : shown)
        {
            const Actor& actor = *shownActor;
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
            if (actor.brain.has_value() && !actor.machine.has_value())
            {
                info.npcState = actor.brain->state;
            }
            if (actor.machine.has_value())
            {
                info.machineState = activeNpcMachineState(*actor.machine).name;
            }
            else if (actor.brain.has_value())
            {
                info.npcTactic = actor.brain->tactic;
            }
            if (actor.pathFollower.has_value())
            {
                info.pathFollower = pathFollowerDebugInfo(
                    actor, map, actor.pathFollower.value(), simulationStepSeconds);
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

        for (const Projectile& projectile : world.projectiles())
        {
            if (overlaps(projectile.bounds, view))
            {
                scene.projectiles.push_back(
                    {projectile.bounds, projectile.lifetimeRemaining, projectile.owner});
            }
        }

        for (const Pickup& pickup : world.pickups())
        {
            if (overlaps(pickup.body.bounds, view))
            {
                scene.pickups.push_back(
                    {pickup.body.bounds, world.itemDefinition(pickup.stack.item).name});
            }
        }

        const Actor* underCursor = machineNpcUnderCursor(shown, navigation.cursorWorld);
        const Actor* followed = nullptr;
        if (lockedMachineActor.has_value())
        {
            const Actor* locked = world.findActor(*lockedMachineActor);
            if (locked != nullptr && locked->machine.has_value())
            {
                followed = locked;
            }
        }
        else if (underCursor != nullptr)
        {
            followed = underCursor;
        }
        else
        {
            followed = nearestNpcWithMachine(
                shown, player != nullptr ? player->body.bounds : scene.cameraBounds);
        }
        if (followed != nullptr)
        {
            const NpcMachine& machine = followed->machine.value_or(NpcMachine{});
            scene.machine = MachineDebugInfo{
                followed->id, machine.definition, machine.active, machine.lastFired};
        }

        scene.navigationCache =
            makeNavigationCacheDebugInfo(world, map, simulationStepSeconds, navigation, view);
        scene.breakableCellUnderCursor = breakableCellUnderCursor(map, navigation.cursorWorld);
        return scene;
    }
}
