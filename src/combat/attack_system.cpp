#include "simple_platformer/combat/attack_system.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/validation.hpp"
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
        const glm::vec2 direction = glm::normalize(actor.intentions.aimDirection);
        const glm::vec2 actorCenter = simple_platformer::centerOf(actor.body.bounds);
        const float actorRadius =
            std::max(actor.body.bounds.size.x, actor.body.bounds.size.y) * 0.5F;
        const float projectileRadius =
            std::max(weapon.projectileSize.x, weapon.projectileSize.y) * 0.5F;
        const glm::vec2 projectileCenter =
            actorCenter + direction * (actorRadius + projectileRadius);

        simple_platformer::Projectile projectile;
        projectile.bounds = {
            projectileCenter - weapon.projectileSize * 0.5F, weapon.projectileSize};
        projectile.velocity = direction * weapon.projectileSpeed;
        projectile.damage = weapon.damage;
        projectile.remainingLifetime = weapon.projectileLifetime;
        projectile.owner = actor.id;
        projectile.team = actor.team;
        projectile.sprite = weapon.projectileSprite;
        return projectile;
    }

    void beginShot(
        const simple_platformer::Actor& actor,
        simple_platformer::RangedWeapon& weapon,
        simple_platformer::WorldRequests& requests)
    {
        weapon.phase = simple_platformer::RangedPhase::Shoot;
        weapon.phaseTimeRemaining = weapon.shootDuration;
        weapon.firedThisUpdate = true;
        requests.spawnProjectile(makeProjectile(actor, weapon));
    }

    void enterNextPhase(simple_platformer::RangedWeapon& weapon)
    {
        using simple_platformer::RangedPhase;

        switch (weapon.phase)
        {
        case RangedPhase::Ready:
            return;
        case RangedPhase::Shoot:
            weapon.phase = RangedPhase::Recovery;
            weapon.phaseTimeRemaining = weapon.recoveryDuration;
            return;
        case RangedPhase::Recovery:
            weapon.phase = RangedPhase::Ready;
            weapon.phaseTimeRemaining = 0.0F;
            return;
        }
    }

    void advanceShot(simple_platformer::RangedWeapon& weapon, float deltaTime)
    {
        float remaining = deltaTime;
        while (weapon.phase != simple_platformer::RangedPhase::Ready &&
               remaining >= weapon.phaseTimeRemaining)
        {
            remaining -= weapon.phaseTimeRemaining;
            enterNextPhase(weapon);
        }

        if (weapon.phase != simple_platformer::RangedPhase::Ready)
        {
            weapon.phaseTimeRemaining -= remaining;
        }
    }

    bool hasAimDirection(const simple_platformer::Actor& actor)
    {
        if (!simple_platformer::isFinite(actor.intentions.aimDirection))
        {
            return false;
        }
        const float lengthSquared =
            glm::dot(actor.intentions.aimDirection, actor.intentions.aimDirection);
        return std::isfinite(lengthSquared) && lengthSquared > 0.0F;
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
                weapon.firedThisUpdate = false;
                if (actor.life != LifeState::Alive)
                {
                    weapon.phase = RangedPhase::Ready;
                    weapon.phaseTimeRemaining = 0.0F;
                }
                else if (
                    weapon.phase == RangedPhase::Ready && actor.intentions.primaryAttackPressed &&
                    hasAimDirection(actor))
                {
                    beginShot(actor, weapon, requests);
                }
                else
                {
                    advanceShot(weapon, deltaTime);
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
