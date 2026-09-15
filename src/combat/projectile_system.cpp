#include "simple_platformer/combat/projectile_system.hpp"

#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>

#include <glm/common.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/physics/segment_cast.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"

namespace
{
    simple_platformer::Aabb expandedForProjectile(
        const simple_platformer::Aabb& target,
        glm::vec2 projectileSize)
    {
        const glm::vec2 halfSize = projectileSize * 0.5F;
        return {target.position - halfSize, target.size + projectileSize};
    }

    std::optional<float> earliestTileHit(
        const simple_platformer::TileMap& map,
        const simple_platformer::Projectile& projectile,
        glm::vec2 start,
        glm::vec2 end)
    {
        const glm::vec2 halfSize = projectile.bounds.size * 0.5F;
        const glm::vec2 minimum = glm::min(start, end) - halfSize;
        const glm::vec2 maximum = glm::max(start, end) + halfSize;
        const float tileSize = static_cast<float>(simple_platformer::TileSize);
        const int firstColumn = static_cast<int>(std::floor(minimum.x / tileSize));
        const int lastColumn = static_cast<int>(std::floor(maximum.x / tileSize));
        const int firstRow = static_cast<int>(std::floor(minimum.y / tileSize));
        const int lastRow = static_cast<int>(std::floor(maximum.y / tileSize));

        std::optional<float> earliest;
        for (int row = firstRow; row <= lastRow; ++row)
        {
            for (int column = firstColumn; column <= lastColumn; ++column)
            {
                if (!map.blocksMovement({column, row}))
                {
                    continue;
                }

                const simple_platformer::Aabb tile{
                    {static_cast<float>(column * simple_platformer::TileSize),
                     static_cast<float>(row * simple_platformer::TileSize)},
                    {tileSize, tileSize}};
                const std::optional<float> hit = simple_platformer::segmentCast(
                    expandedForProjectile(tile, projectile.bounds.size), start, end);
                if (hit.has_value() && (!earliest.has_value() || *hit < *earliest))
                {
                    earliest = hit;
                }
            }
        }
        return earliest;
    }
}

namespace simple_platformer
{
    void updateProjectiles(
        const TileMap& map,
        World& world,
        WorldRequests& requests,
        float deltaTime)
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0F)
        {
            throw std::invalid_argument("Projectile delta time must be finite and non-negative");
        }

        for (std::size_t index = 0; index < world.projectiles().size(); ++index)
        {
            Projectile& projectile = world.projectiles()[index];
            projectile.remainingLifetime -= deltaTime;
            if (projectile.remainingLifetime <= 0.0F)
            {
                requests.removeProjectile(index);
                continue;
            }

            const glm::vec2 start = centerOf(projectile.bounds);
            const glm::vec2 end = start + projectile.velocity * deltaTime;
            const std::optional<float> tileHit = earliestTileHit(map, projectile, start, end);
            float earliest = tileHit.value_or(1.0F);
            std::optional<ActorId> actorHit;

            for (const Actor& actor : world.actors())
            {
                if (actor.life != LifeState::Alive || !actor.health.has_value() ||
                    !areOpponents(projectile.team, actor.team) || projectile.owner == actor.id)
                {
                    continue;
                }

                const std::optional<float> hit = segmentCast(
                    expandedForProjectile(actor.body.bounds, projectile.bounds.size), start, end);
                if (hit.has_value() &&
                    (*hit < earliest ||
                     (!tileHit.has_value() && !actorHit.has_value() && *hit == earliest)))
                {
                    earliest = *hit;
                    actorHit = actor.id;
                }
            }

            const glm::vec2 impactCenter = start + (end - start) * earliest;
            projectile.bounds.position = impactCenter - projectile.bounds.size * 0.5F;
            if (actorHit.has_value())
            {
                requests.damage(actorHit.value_or(ActorId{}), projectile.damage);
                requests.removeProjectile(index);
            }
            else if (tileHit.has_value())
            {
                requests.removeProjectile(index);
            }
        }
    }
}
