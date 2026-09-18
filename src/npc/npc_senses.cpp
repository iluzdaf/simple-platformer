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
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/physics/segment_cast.hpp"
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
    }

    bool canSeeTarget(
        const TileMap& map,
        const Aabb& observer,
        const Aabb& target,
        const NpcSenses& senses)
    {
        if (!std::isfinite(senses.noticeDistance) || senses.noticeDistance < 0.0F)
        {
            throw std::invalid_argument("NPC notice distance must be finite and non-negative");
        }

        const glm::vec2 start = centerOf(observer);
        const glm::vec2 end = centerOf(target);
        const glm::vec2 offset = end - start;
        if (glm::dot(offset, offset) > senses.noticeDistance * senses.noticeDistance)
        {
            return false;
        }

        return !segmentCastSolidTiles(map, start, end).has_value();
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
            if (actor.life == LifeState::Alive && livingPlayer &&
                areOpponents(actor.team, player->team) &&
                canSeeTarget(map, actor.body.bounds, player->body.bounds, *actor.senses))
            {
                brain.target = player->id;
                brain.lastSeenTargetFeet = feetOf(player->body.bounds);
                brain.targetMemoryRemaining = actor.senses->forgetAfter;
                brain.targetVisible = true;
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
