#include "simple_platformer/render/render_scene.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/world/level_exit.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    namespace
    {
        constexpr float DeathFadeDurationSeconds = 0.2F;
        constexpr float HitFlashDurationSeconds = 0.1F;
        constexpr float HitFlashAmount = 0.1F;
        constexpr float PickupBobHeight = 2.0F;
        constexpr float PickupBobPeriodSeconds = 1.0F;
        constexpr int PickupBobPhaseCount = 4;
        constexpr float ProjectileBurstFinalScale = 2.0F;
        constexpr float RadiansPerCycle = 6.28318530717958647692F;

        float actorOpacity(const Actor& actor)
        {
            if (actor.life == LifeState::Alive)
            {
                return 1.0F;
            }
            return std::clamp(actor.deathTimeRemaining / DeathFadeDurationSeconds, 0.0F, 1.0F);
        }

        float actorWhiteFlashAmount(const Actor& actor, float simulationTimeSeconds)
        {
            const bool wasRecentlyDamaged =
                actor.lastDamageTimeSeconds.has_value() &&
                simulationTimeSeconds - actor.lastDamageTimeSeconds.value() <
                    HitFlashDurationSeconds;
            return wasRecentlyDamaged ? HitFlashAmount : 0.0F;
        }

        float pickupVerticalOffset(float animationTime)
        {
            const float cycleRadians = animationTime * RadiansPerCycle / PickupBobPeriodSeconds;
            return -PickupBobHeight * 0.5F * (1.0F - std::cos(cycleRadians));
        }

        float pickupPhaseOffset(int tileSize, const Aabb& bounds)
        {
            // Spread level-start pickups across four phases instead of bobbing in lockstep.
            const GridPosition cell = worldToGrid(tileSize, bounds.position);
            int phaseIndex = (cell.x + cell.y) % PickupBobPhaseCount;
            if (phaseIndex < 0)
            {
                phaseIndex += PickupBobPhaseCount;
            }
            return static_cast<float>(phaseIndex) * PickupBobPeriodSeconds /
                   static_cast<float>(PickupBobPhaseCount);
        }

        void appendTiles(
            RenderScene& scene,
            const TileMap& map,
            int tileTextureId,
            const Camera& camera)
        {
            const float tileSize = static_cast<float>(map.tileSize());
            const int firstColumn =
                std::max(0, static_cast<int>(std::floor(camera.position.x / tileSize)));
            const int lastColumn = std::min(
                map.width() - 1,
                static_cast<int>(
                    std::ceil((camera.position.x + camera.viewportSize.x) / tileSize)) -
                    1);
            const int firstRow =
                std::max(0, static_cast<int>(std::floor(camera.position.y / tileSize)));
            const int lastRow = std::min(
                map.height() - 1,
                static_cast<int>(
                    std::ceil((camera.position.y + camera.viewportSize.y) / tileSize)) -
                    1);

            for (int row = firstRow; row <= lastRow; ++row)
            {
                for (int column = firstColumn; column <= lastColumn; ++column)
                {
                    const GridPosition tilePosition{column, row};
                    if (map.tileAt(tilePosition) == 0)
                    {
                        continue;
                    }

                    const glm::vec2 worldPosition = {
                        static_cast<float>(column * map.tileSize()),
                        static_cast<float>(row * map.tileSize())};
                    scene.sprites.push_back(
                        {tileTextureId,
                         worldToScreen(camera, worldPosition),
                         {tileSize, tileSize},
                         map.definitionAt(tilePosition).sprite,
                         false});
                }
            }
        }

        void appendPickups(
            RenderScene& scene,
            const TileMap& map,
            const World& world,
            const Camera& camera)
        {
            for (const Pickup& pickup : world.pickups())
            {
                const float pickupVisibility = pickup.screenVisibility.value_or(1.0F);
                if (pickupVisibility <= 0.0F)
                {
                    continue;
                }
                const Sprite& sprite =
                    pickup.sprite ? *pickup.sprite : world.itemDefinition(pickup.stack.item).icon;
                Aabb bounds = spriteBounds(pickup.bounds, sprite);
                bounds.position.y += pickupVerticalOffset(
                    world.simulationTimeSeconds() +
                    pickupPhaseOffset(map.tileSize(), pickup.bounds));
                scene.sprites.push_back(
                    {sprite.textureId,
                     worldToScreen(camera, bounds.position),
                     bounds.size,
                     sprite.region,
                     false,
                     0.0F,
                     pickupVisibility});
            }
        }

        void appendExit(RenderScene& scene, const World& world, const Camera& camera)
        {
            const auto& exit = world.exit();
            if (!exit.has_value())
            {
                return;
            }

            const LevelExit& levelExit = exit.value();
            if (!levelExit.sprite.has_value())
            {
                return;
            }

            const Sprite& sprite = levelExit.sprite.value();
            const Aabb bounds = spriteBounds(levelExit.bounds, sprite);
            scene.sprites.push_back(
                {sprite.textureId,
                 worldToScreen(camera, bounds.position),
                 bounds.size,
                 sprite.region,
                 false});
        }

        void appendActors(RenderScene& scene, const World& world, const Camera& camera)
        {
            for (const Actor& actor : world.actors())
            {
                if (!actor.sprite.has_value())
                {
                    continue;
                }
                const float actorVisibility = actor.screenVisibility.value_or(1.0F);
                if (actorVisibility <= 0.0F)
                {
                    continue;
                }

                const Aabb bounds = spriteBounds(actor.body.bounds, *actor.sprite);
                scene.sprites.push_back(
                    {actor.sprite->textureId,
                     worldToScreen(camera, bounds.position),
                     bounds.size,
                     actor.sprite->region,
                     actor.facing == Facing::Left,
                     0.0F,
                     actorOpacity(actor) * actorVisibility,
                     actorWhiteFlashAmount(actor, world.simulationTimeSeconds())});
            }
        }

        void appendProjectiles(RenderScene& scene, const World& world, const Camera& camera)
        {
            for (const Projectile& projectile : world.projectiles())
            {
                const glm::vec2 projectileCenter = centerOf(projectile.bounds);
                const glm::vec2 spritePosition = projectileCenter - projectile.sprite.size * 0.5F;
                const float rotationRadians =
                    std::atan2(projectile.velocity.y, projectile.velocity.x);
                scene.sprites.push_back(
                    {projectile.sprite.textureId,
                     worldToScreen(camera, spritePosition),
                     projectile.sprite.size,
                     projectile.sprite.region,
                     false,
                     rotationRadians});
            }
        }

        void appendProjectileBursts(RenderScene& scene, const World& world, const Camera& camera)
        {
            for (const ProjectileBurst& burst : world.projectileBursts())
            {
                const float remainingFraction =
                    std::clamp(burst.remainingLifetime / burst.duration, 0.0F, 1.0F);
                const float progress = 1.0F - remainingFraction;
                const float scale = 1.0F + progress * (ProjectileBurstFinalScale - 1.0F);
                const glm::vec2 size = burst.sprite.size * scale;
                const glm::vec2 position = burst.center - size * 0.5F;
                const float rotationRadians = std::atan2(burst.direction.y, burst.direction.x);
                scene.sprites.push_back(
                    {burst.sprite.textureId,
                     worldToScreen(camera, position),
                     size,
                     burst.sprite.region,
                     false,
                     rotationRadians,
                     remainingFraction});
            }
        }
    }

    RenderScene buildRenderScene(
        const TileMap& map,
        int tileTextureId,
        const Camera& camera,
        const World& world)
    {
        RenderScene scene;
        appendTiles(scene, map, tileTextureId, camera);
        appendPickups(scene, map, world, camera);
        appendExit(scene, world, camera);
        appendActors(scene, world, camera);
        appendProjectiles(scene, world, camera);
        appendProjectileBursts(scene, world, camera);
        return scene;
    }
}
