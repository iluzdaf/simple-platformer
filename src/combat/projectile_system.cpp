#include "simple_platformer/combat/projectile_system.hpp"

#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>

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
        Aabb expandedForProjectile(const Aabb& target, glm::vec2 projectileSize)
        {
            const glm::vec2 halfSize = projectileSize * 0.5F;
            return {target.position - halfSize, target.size + projectileSize};
        }
    }

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
            const std::optional<float> tileHit =
                segmentCastSolidTiles(map, start, end, projectile.bounds.size);
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
