#pragma once

#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    enum class Team
    {
        Neutral,
        Player,
        Enemy
    };

    bool areOpponents(Team first, Team second);

    enum class RangedPhase
    {
        Ready,
        Shoot,
        Recovery
    };

    struct RangedWeapon
    {
        int damage = 1;
        // Collision dimensions measured in world pixels.
        glm::vec2 projectileSize = {4.0F, 2.0F};
        float projectileSpeed = 180.0F;
        float projectileLifetime = 2.0F;
        float shootDuration = 0.15F;
        float recoveryDuration = 0.20F;

        RangedPhase phase = RangedPhase::Ready;
        float phaseTimeRemaining = 0.0F;
        bool firedThisUpdate = false;
        // Its display size is independent of projectileSize, just like an actor sprite and body.
        Sprite projectileSprite = {0, {}, {4.0F, 2.0F}};
    };

    enum class BitePhase
    {
        Ready,
        Windup,
        Active,
        Recovery
    };

    struct BiteAttack
    {
        int damage = 1;
        // The active collision box is placed reach pixels beyond the actor's facing edge.
        glm::vec2 hitboxSize = {10.0F, 8.0F};
        float reach = 4.0F;
        float windupDuration = 0.12F;
        float activeDuration = 0.08F;
        float recoveryDuration = 0.30F;

        BitePhase phase = BitePhase::Ready;
        float phaseTimeRemaining = 0.0F;
        std::vector<ActorId> actorsHit;
    };

    struct Projectile
    {
        Aabb bounds;
        glm::vec2 velocity = {0.0F, 0.0F};
        int damage = 1;
        float remainingLifetime = 1.0F;
        std::optional<ActorId> owner;
        Team team = Team::Neutral;
        Sprite sprite;
    };

    enum class ProjectileBurstCause
    {
        Impact,
        LifetimeExpired
    };

    struct ProjectileBurst
    {
        ProjectileBurstCause cause = ProjectileBurstCause::Impact;
        glm::vec2 center = {0.0F, 0.0F};
        glm::vec2 direction = {1.0F, 0.0F};
        Sprite sprite;
        float duration = 0.1F;
        float remainingLifetime = 0.1F;
    };
}
