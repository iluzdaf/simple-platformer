#include "simple_platformer/combat/attack_system.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"

namespace
{
    bool overlaps(const simple_platformer::Aabb& first, const simple_platformer::Aabb& second)
    {
        return first.position.x < second.position.x + second.size.x &&
               first.position.x + first.size.x > second.position.x &&
               first.position.y < second.position.y + second.size.y &&
               first.position.y + first.size.y > second.position.y;
    }

    bool hasHit(const simple_platformer::BiteAttack& bite, simple_platformer::ActorId id)
    {
        return std::find(bite.actorsHit.begin(), bite.actorsHit.end(), id) != bite.actorsHit.end();
    }

    simple_platformer::Projectile makeProjectile(
        const simple_platformer::Actor& actor,
        const simple_platformer::RangedWeapon& weapon)
    {
        const float direction = actor.facing == simple_platformer::Facing::Left ? -1.0F : 1.0F;
        const glm::vec2 actorCenter = simple_platformer::centerOf(actor.body.bounds);
        const float left = direction > 0.0F
                               ? actor.body.bounds.position.x + actor.body.bounds.size.x
                               : actor.body.bounds.position.x - weapon.projectileSize.x;

        simple_platformer::Projectile projectile;
        projectile.bounds = {
            {left, actorCenter.y - weapon.projectileSize.y * 0.5F}, weapon.projectileSize};
        projectile.velocity = {direction * weapon.projectileSpeed, 0.0F};
        projectile.damage = weapon.damage;
        projectile.remainingLifetime = weapon.projectileLifetime;
        projectile.owner = actor.id;
        projectile.team = actor.team;
        projectile.sprite = weapon.projectileSprite;
        return projectile;
    }

    void beginBite(simple_platformer::BiteAttack& bite)
    {
        bite.phase = simple_platformer::BitePhase::Windup;
        bite.phaseTimeRemaining = bite.windupDuration;
        bite.actorsHit.clear();
    }

    void enterNextPhase(simple_platformer::BiteAttack& bite)
    {
        using simple_platformer::BitePhase;

        switch (bite.phase)
        {
        case BitePhase::Ready:
            return;
        case BitePhase::Windup:
            bite.phase = BitePhase::Active;
            bite.phaseTimeRemaining = bite.activeDuration;
            return;
        case BitePhase::Active:
            bite.phase = BitePhase::Recovery;
            bite.phaseTimeRemaining = bite.recoveryDuration;
            return;
        case BitePhase::Recovery:
            bite.phase = BitePhase::Ready;
            bite.phaseTimeRemaining = 0.0F;
            return;
        }
    }

    bool advanceBite(simple_platformer::BiteAttack& bite, float deltaTime)
    {
        bool activeDuringUpdate = bite.phase == simple_platformer::BitePhase::Active;
        float remaining = deltaTime;
        while (bite.phase != simple_platformer::BitePhase::Ready &&
               remaining >= bite.phaseTimeRemaining)
        {
            remaining -= bite.phaseTimeRemaining;
            enterNextPhase(bite);
            activeDuringUpdate =
                activeDuringUpdate || bite.phase == simple_platformer::BitePhase::Active;
        }

        if (bite.phase != simple_platformer::BitePhase::Ready)
        {
            bite.phaseTimeRemaining -= remaining;
        }
        return activeDuringUpdate;
    }
}

namespace simple_platformer
{
    Aabb biteHitbox(const Aabb& actorBounds, const BiteAttack& bite, Facing facing)
    {
        const float left = facing == Facing::Right
                               ? actorBounds.position.x + actorBounds.size.x + bite.reach
                               : actorBounds.position.x - bite.reach - bite.hitboxSize.x;
        return {{left, centerOf(actorBounds).y - bite.hitboxSize.y * 0.5F}, bite.hitboxSize};
    }

    void updateAttacks(World& world, WorldRequests& requests, float deltaTime)
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0F)
        {
            throw std::invalid_argument("Attack delta time must be finite and non-negative");
        }

        for (Actor& actor : world.actors())
        {
            if (actor.rangedWeapon.has_value())
            {
                RangedWeapon& weapon = *actor.rangedWeapon;
                weapon.cooldownRemaining = std::max(0.0F, weapon.cooldownRemaining - deltaTime);
                if (actor.life == LifeState::Alive && actor.intentions.primaryAttackPressed &&
                    weapon.cooldownRemaining == 0.0F)
                {
                    requests.spawnProjectile(makeProjectile(actor, weapon));
                    weapon.cooldownRemaining = weapon.cooldown;
                }
            }

            if (!actor.bite.has_value())
            {
                continue;
            }

            BiteAttack& bite = *actor.bite;
            if (actor.life != LifeState::Alive)
            {
                bite.phase = BitePhase::Ready;
                bite.phaseTimeRemaining = 0.0F;
                bite.actorsHit.clear();
                continue;
            }

            bool activeDuringUpdate = false;
            if (bite.phase == BitePhase::Ready && actor.intentions.primaryAttackPressed)
            {
                beginBite(bite);
            }
            else
            {
                activeDuringUpdate = advanceBite(bite, deltaTime);
            }

            if (!activeDuringUpdate)
            {
                continue;
            }

            const Aabb hitbox = biteHitbox(actor.body.bounds, bite, actor.facing);
            for (const Actor& target : world.actors())
            {
                if (target.id == actor.id || target.life != LifeState::Alive ||
                    !target.health.has_value() || !areOpponents(actor.team, target.team) ||
                    hasHit(bite, target.id) || !overlaps(hitbox, target.body.bounds))
                {
                    continue;
                }

                requests.damage(target.id, bite.damage);
                bite.actorsHit.push_back(target.id);
            }
        }
    }
}
