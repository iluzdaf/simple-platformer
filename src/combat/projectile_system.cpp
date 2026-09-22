#include "simple_platformer/combat/projectile_system.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <vector>

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

namespace simple_platformer
{
    namespace
    {
        struct ProjectileHit
        {
            float segmentTime = 1.0F;
            std::optional<ActorId> actor;
            std::optional<GridPosition> tile;

            bool occurred() const
            {
                return actor.has_value() || tile.has_value();
            }
        };

        Aabb expandedForProjectile(const Aabb& target, glm::vec2 projectileSize)
        {
            const glm::vec2 halfSize = projectileSize * 0.5F;
            return {target.position - halfSize, target.size + projectileSize};
        }

        ProjectileBurst makeBurst(
            const Projectile& projectile,
            glm::vec2 center,
            ProjectileBurstCause cause)
        {
            ProjectileBurst burst;
            burst.cause = cause;
            burst.center = center;
            burst.direction = projectile.velocity;
            if (burst.direction.x == 0.0F && burst.direction.y == 0.0F)
            {
                burst.direction = {1.0F, 0.0F};
            }
            burst.sprite = projectile.sprite;
            return burst;
        }

        ProjectileHit findEarliestHit(
            const TileMap& map,
            const World& world,
            const Projectile& projectile,
            glm::vec2 start,
            glm::vec2 end)
        {
            ProjectileHit earliest;
            const std::optional<TileSegmentHit> tileHit =
                segmentCastMovementBlockingTiles(map, start, end, projectile.bounds.size);
            if (tileHit.has_value())
            {
                earliest.segmentTime = tileHit->segmentTime;
                earliest.tile = tileHit->cell;
            }

            for (const Actor& actor : world.actors())
            {
                if (actor.life != LifeState::Alive || !actor.health.has_value() ||
                    !areOpponents(projectile.team, actor.team) || projectile.owner == actor.id)
                {
                    continue;
                }

                const std::optional<float> actorHit = segmentCast(
                    expandedForProjectile(actor.body.bounds, projectile.bounds.size), start, end);
                if (actorHit.has_value() &&
                    (*actorHit < earliest.segmentTime ||
                     (!earliest.occurred() && *actorHit == earliest.segmentTime)))
                {
                    earliest.segmentTime = *actorHit;
                    earliest.actor = actor.id;
                    earliest.tile.reset();
                }
            }
            return earliest;
        }

        void endProjectile(
            WorldRequests& requests,
            const Projectile& projectile,
            std::size_t index,
            glm::vec2 center,
            ProjectileBurstCause cause)
        {
            requests.spawnProjectileBurst(makeBurst(projectile, center, cause));
            requests.removeProjectile(index);
        }
    }

    void updateProjectiles(TileMap& map, World& world, WorldRequests& requests, float deltaTime)
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0F)
        {
            throw std::invalid_argument("Projectile delta time must be finite and non-negative");
        }

        // The map belongs to GameLevel, not World, so breaks are not WorldRequests. They are
        // still held back until every shot has been traced, so one shot cannot open a hole
        // that a later shot in the same frame flies through.
        std::vector<GridPosition> brokenTiles;
        for (std::size_t index = 0; index < world.projectiles().size(); ++index)
        {
            Projectile& projectile = world.projectiles()[index];
            const glm::vec2 start = centerOf(projectile.bounds);
            const float travelTime = std::min(deltaTime, projectile.lifetimeRemaining);
            const glm::vec2 end = start + projectile.velocity * travelTime;
            const ProjectileHit hit = findEarliestHit(map, world, projectile, start, end);
            const glm::vec2 finalCenter = start + (end - start) * hit.segmentTime;
            projectile.bounds.position = finalCenter - projectile.bounds.size * 0.5F;

            if (hit.actor.has_value())
            {
                requests.damage(*hit.actor, projectile.damage);
            }
            if (hit.tile.has_value() && projectile.breaksTiles)
            {
                brokenTiles.push_back(*hit.tile);
            }
            if (hit.occurred())
            {
                endProjectile(
                    requests, projectile, index, finalCenter, ProjectileBurstCause::Impact);
            }
            else
            {
                projectile.lifetimeRemaining -= deltaTime;
                if (projectile.lifetimeRemaining <= 0.0F)
                {
                    endProjectile(
                        requests,
                        projectile,
                        index,
                        finalCenter,
                        ProjectileBurstCause::LifetimeExpired);
                }
            }
        }

        for (const GridPosition cell : brokenTiles)
        {
            map.breakTile(cell);
        }
    }

    void updateProjectileBursts(World& world, WorldRequests& requests, float deltaTime)
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0F)
        {
            throw std::invalid_argument(
                "Projectile burst delta time must be finite and non-negative");
        }

        for (std::size_t index = 0; index < world.projectileBursts().size(); ++index)
        {
            ProjectileBurst& burst = world.projectileBursts()[index];
            burst.lifetimeRemaining -= deltaTime;
            if (burst.lifetimeRemaining <= 0.0F)
            {
                requests.removeProjectileBurst(index);
            }
        }
    }
}
