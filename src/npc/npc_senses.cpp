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

namespace
{
    bool tileBlocksSight(
        const simple_platformer::TileMap& map,
        simple_platformer::GridPosition position,
        glm::vec2 start,
        glm::vec2 end)
    {
        if (!map.isSolid(position))
        {
            return false;
        }
        return simple_platformer::segmentCast(
                   {simple_platformer::gridToWorld(position),
                    {static_cast<float>(simple_platformer::TileSize),
                     static_cast<float>(simple_platformer::TileSize)}},
                   start,
                   end)
            .has_value();
    }

    const simple_platformer::Actor* livingTarget(
        const simple_platformer::World& world,
        const simple_platformer::NpcBrain& brain)
    {
        const simple_platformer::Actor* target =
            world.findActor(brain.target.value_or(simple_platformer::ActorId{}));
        return target != nullptr && target->life == simple_platformer::LifeState::Alive ? target
                                                                                        : nullptr;
    }
}

namespace simple_platformer
{
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

        for (int y = 0; y < map.height(); ++y)
        {
            for (int x = 0; x < map.width(); ++x)
            {
                if (tileBlocksSight(map, {x, y}, start, end))
                {
                    return false;
                }
            }
        }
        return true;
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
