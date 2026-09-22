#include "simple_platformer/npc/npc_senses.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/world/sight.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    namespace
    {
        const Actor* livingTarget(const World& world, const NpcBrain& brain)
        {
            const Actor* target = world.findActor(brain.target.value_or(ActorId{}));
            return target != nullptr && target->life == LifeState::Alive ? target : nullptr;
        }

        bool withinNoticeDistance(const Aabb& observer, const Aabb& target, const NpcSenses& senses)
        {
            if (!std::isfinite(senses.noticeDistance) || senses.noticeDistance < 0.0F)
            {
                throw std::invalid_argument("NPC notice distance must be finite and non-negative");
            }

            const glm::vec2 offset = centerOf(target) - centerOf(observer);
            return glm::dot(offset, offset) <= senses.noticeDistance * senses.noticeDistance;
        }

        // Senses run before attacks in an update, so a shot fired since senses last ran is
        // stamped one step ago. A tenth of a step of slack covers the clock's rounding.
        bool firedSinceLastSensing(const Actor& actor, float now, float deltaTime)
        {
            if (!actor.rangedWeapon.has_value() ||
                !actor.rangedWeapon->lastFiredTimeSeconds.has_value())
            {
                return false;
            }
            const float age = now - *actor.rangedWeapon->lastFiredTimeSeconds;
            return age >= 0.0F && age <= deltaTime * 1.1F;
        }

        void rememberTarget(NpcBrain& brain, const Actor& target, const NpcSenses& senses)
        {
            brain.target = target.id;
            brain.lastSeenTargetFeet = feetOf(target.body.bounds);
            brain.targetMemoryRemaining = senses.targetMemoryDuration;
        }
    }

    bool canSeeTarget(
        const TileMap& map,
        const Aabb& observer,
        const Aabb& target,
        const NpcSenses& senses)
    {
        if (!withinNoticeDistance(observer, target, senses))
        {
            return false;
        }

        return lineOfSight(map, centerOf(observer), centerOf(target));
    }

    bool seenByAnyNpc(const TileMap& map, const World& world, const Actor& target)
    {
        for (const Actor& actor : world.actors())
        {
            if (actor.id == target.id || !actor.senses.has_value() ||
                actor.life != LifeState::Alive)
            {
                continue;
            }
            if (canSeeTarget(map, actor.body.bounds, target.body.bounds, *actor.senses))
            {
                return true;
            }
        }
        return false;
    }

    void updateNpcSenses(const TileMap& map, World& world, float deltaTime)
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0F)
        {
            throw std::invalid_argument("NPC sensing time must be finite and non-negative");
        }

        const Actor* player = world.findActor(world.playerId());
        for (Actor& actor : world.actors())
        {
            if (!actor.brain.has_value())
            {
                continue;
            }
            if (!actor.senses.has_value())
            {
                throw std::logic_error("An NPC is missing its senses");
            }

            NpcBrain& brain = *actor.brain;
            brain.targetVisible = false;
            const bool livingPlayer = player != nullptr && player->life == LifeState::Alive;
            const bool sensesPlayer = actor.life == LifeState::Alive && livingPlayer &&
                                      areOpponents(actor.team, player->team);
            if (sensesPlayer &&
                canSeeTarget(map, actor.body.bounds, player->body.bounds, *actor.senses))
            {
                rememberTarget(brain, *player, *actor.senses);
                brain.targetVisible = true;
                continue;
            }
            // A shot is heard through anything within notice distance, and remembers where
            // the player fired from without making them visible.
            if (sensesPlayer &&
                firedSinceLastSensing(*player, world.simulationTimeSeconds(), deltaTime) &&
                withinNoticeDistance(actor.body.bounds, player->body.bounds, *actor.senses))
            {
                rememberTarget(brain, *player, *actor.senses);
                continue;
            }

            const Actor* remembered = livingTarget(world, brain);
            if (remembered == nullptr)
            {
                brain.target.reset();
                brain.targetMemoryRemaining = 0.0F;
                continue;
            }

            brain.targetMemoryRemaining = std::max(0.0F, brain.targetMemoryRemaining - deltaTime);
            if (brain.targetMemoryRemaining == 0.0F)
            {
                brain.target.reset();
            }
        }
    }
}
