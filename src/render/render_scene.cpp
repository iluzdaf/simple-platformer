#include "simple_platformer/render/render_scene.hpp"

#include <algorithm>
#include <cmath>

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

namespace
{
    constexpr float PickupBobHeight = 2.0F;
    constexpr float PickupBobPeriodSeconds = 1.0F;
    constexpr int PickupBobPhaseCount = 4;
    constexpr float RadiansPerCycle = 6.28318530717958647692F;

    float pickupVerticalOffset(float animationTime)
    {
        const float cycleRadians = animationTime * RadiansPerCycle / PickupBobPeriodSeconds;
        return -PickupBobHeight * 0.5F * (1.0F - std::cos(cycleRadians));
    }

    float pickupPhaseOffset(const simple_platformer::Aabb& bounds)
    {
        // Spread level-start pickups across four phases instead of bobbing in lockstep.
        const simple_platformer::GridPosition cell =
            simple_platformer::worldToGrid(bounds.position);
        int phaseIndex = (cell.x + cell.y) % PickupBobPhaseCount;
        if (phaseIndex < 0)
        {
            phaseIndex += PickupBobPhaseCount;
        }
        return static_cast<float>(phaseIndex) * PickupBobPeriodSeconds /
               static_cast<float>(PickupBobPhaseCount);
    }
}

namespace simple_platformer
{
    RenderScene buildRenderScene(
        const TileMap& map,
        int tileTextureId,
        const Camera& camera,
        const World& world)
    {
        RenderScene scene;
        const float tileSize = static_cast<float>(TileSize);
        const int firstColumn =
            std::max(0, static_cast<int>(std::floor(camera.position.x / tileSize)));
        const int lastColumn = std::min(
            map.width() - 1,
            static_cast<int>(std::ceil((camera.position.x + camera.viewportSize.x) / tileSize)) -
                1);
        const int firstRow =
            std::max(0, static_cast<int>(std::floor(camera.position.y / tileSize)));
        const int lastRow = std::min(
            map.height() - 1,
            static_cast<int>(std::ceil((camera.position.y + camera.viewportSize.y) / tileSize)) -
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
                    static_cast<float>(column * TileSize), static_cast<float>(row * TileSize)};
                scene.sprites.push_back(
                    {tileTextureId,
                     worldToScreen(camera, worldPosition),
                     {tileSize, tileSize},
                     map.definitionAt(tilePosition).sprite,
                     false});
            }
        }

        for (const Pickup& pickup : world.pickups())
        {
            const Sprite& sprite = world.itemDefinition(pickup.stack.item).icon;
            Aabb bounds = spriteBounds(pickup.bounds, sprite);
            bounds.position.y +=
                pickupVerticalOffset(pickup.ageSeconds + pickupPhaseOffset(pickup.bounds));
            scene.sprites.push_back(
                {sprite.textureId,
                 worldToScreen(camera, bounds.position),
                 bounds.size,
                 sprite.region,
                 false});
        }

        if (world.exit().has_value() && world.exit()->sprite.has_value())
        {
            const Sprite& sprite = *world.exit()->sprite;
            const Aabb bounds = spriteBounds(world.exit()->bounds, sprite);
            scene.sprites.push_back(
                {sprite.textureId,
                 worldToScreen(camera, bounds.position),
                 bounds.size,
                 sprite.region,
                 false});
        }

        for (const Actor& actor : world.actors())
        {
            if (!actor.sprite.has_value())
            {
                continue;
            }

            const Aabb bounds = spriteBounds(actor.body.bounds, *actor.sprite);
            scene.sprites.push_back(
                {actor.sprite->textureId,
                 worldToScreen(camera, bounds.position),
                 bounds.size,
                 actor.sprite->region,
                 actor.facing == Facing::Left});
        }

        for (const Projectile& projectile : world.projectiles())
        {
            const glm::vec2 projectileCenter = centerOf(projectile.bounds);
            const glm::vec2 spritePosition = projectileCenter - projectile.sprite.size * 0.5F;
            const float rotationRadians = std::atan2(projectile.velocity.y, projectile.velocity.x);
            scene.sprites.push_back(
                {projectile.sprite.textureId,
                 worldToScreen(camera, spritePosition),
                 projectile.sprite.size,
                 projectile.sprite.region,
                 false,
                 rotationRadians});
        }
        return scene;
    }
}
