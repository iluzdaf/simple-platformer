#include "simple_platformer/npc/npc_senses.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/navigation/platformer_cells.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/world/sight.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    namespace
    {
        struct SensingNpc
        {
            Actor& actor;
            NpcBrain& brain;
            NpcPerception& perception;
            const NpcSenses& senses;
        };

        bool withinNoticeDistance(const Aabb& observer, const Aabb& target, const NpcSenses& senses)
        {
            if (!std::isfinite(senses.noticeDistance) || senses.noticeDistance < 0.0F)
            {
                throw std::invalid_argument("NPC notice distance must be finite and non-negative");
            }

            const glm::vec2 offset = centerOf(target) - centerOf(observer);
            return glm::dot(offset, offset) <= senses.noticeDistance * senses.noticeDistance;
        }

        void rememberTarget(NpcBrain& brain, const Actor& target, const NpcSenses& senses)
        {
            brain.target = target.id;
            brain.lastKnownTargetFeet = feetOf(target.body.bounds);
            brain.targetMemoryRemaining = senses.targetMemoryDuration;
        }

        bool hearNoises(
            const TileMap& map,
            SensingNpc& npc,
            const Actor& target,
            const std::vector<NoiseEvent>& noises)
        {
            bool heardNoise = false;
            for (const NoiseEvent& noise : noises)
            {
                if (noise.source != target.id)
                {
                    continue;
                }
                Aabb noiseBounds = target.body.bounds;
                placeFeetAt(noiseBounds, noise.feet);
                if (!withinNoticeDistance(npc.actor.body.bounds, noiseBounds, npc.senses))
                {
                    continue;
                }
                // Shots travel through walls; landings are heard only on this run.
                if (noise.kind == NoiseKind::Landing)
                {
                    if (!npc.actor.platformerMovement.has_value() ||
                        !npc.actor.platformerMovement->grounded ||
                        !onSameGroundRun(map, npc.actor.body.bounds, noiseBounds))
                    {
                        continue;
                    }
                    npc.perception.heardLanding = true;
                }
                heardNoise = true;
                rememberTarget(npc.brain, target, npc.senses);
                npc.brain.lastKnownTargetFeet = noise.feet;
            }
            return heardNoise;
        }

        bool observeTarget(const TileMap& map, SensingNpc& npc, const Actor& target)
        {
            if (!canSeeTarget(map, npc.actor.body.bounds, target.body.bounds, npc.senses))
            {
                return false;
            }

            rememberTarget(npc.brain, target, npc.senses);
            npc.perception.targetVisible = true;
            return true;
        }

        void decayTargetMemory(const World& world, NpcBrain& brain, float deltaTime)
        {
            if (livingTarget(world, brain) == nullptr)
            {
                brain.target.reset();
                brain.targetMemoryRemaining = 0.0F;
                return;
            }

            brain.targetMemoryRemaining = std::max(0.0F, brain.targetMemoryRemaining - deltaTime);
            if (brain.targetMemoryRemaining == 0.0F)
            {
                brain.target.reset();
            }
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

    bool onSameGroundRun(const TileMap& map, const Aabb& observer, const Aabb& target)
    {
        const GridPosition first = cellAtFeet(map.tileSize(), feetOf(observer));
        const GridPosition last = cellAtFeet(map.tileSize(), feetOf(target));
        if (first.y != last.y)
        {
            return false;
        }
        for (int x = std::min(first.x, last.x); x <= std::max(first.x, last.x); ++x)
        {
            if (!canStandAt(map, {x, first.y}, observer.size))
            {
                return false;
            }
        }
        return true;
    }

    const Actor* livingTarget(const World& world, const NpcBrain& brain)
    {
        if (!brain.target.has_value())
        {
            return nullptr;
        }
        const Actor* target = world.findActor(*brain.target);
        return target != nullptr && target->life == LifeState::Alive ? target : nullptr;
    }

    bool playerSeenByAnyNpc(const World& world)
    {
        for (const Actor& actor : world.actors())
        {
            if (actor.brain.has_value() && actor.perception.has_value() &&
                actor.perception->targetVisible && actor.brain->target == world.playerId())
            {
                return true;
            }
        }
        return false;
    }

    void updateNpcSenses(const TileMap& map, World& world, float deltaTime)
    {
        requireSeconds(deltaTime, "NPC senses time step");

        const Actor* player = world.findActor(world.playerId());
        const std::vector<NoiseEvent> noises = world.takeNoises();
        for (Actor& actor : world.actors())
        {
            if (!actor.brain.has_value())
            {
                continue;
            }
            if (!actor.senses.has_value() || !actor.perception.has_value())
            {
                throw std::logic_error("An NPC is missing its senses or perception");
            }

            NpcBrain& brain = *actor.brain;
            NpcPerception& perception = *actor.perception;
            perception = {};
            SensingNpc npc{actor, brain, perception, *actor.senses};
            const bool livingPlayer = player != nullptr && player->life == LifeState::Alive;
            const bool sensesPlayer = actor.life == LifeState::Alive && livingPlayer &&
                                      areOpponents(actor.team, player->team);
            bool heardNoise = false;
            bool sawTarget = false;
            if (sensesPlayer)
            {
                // Hearing still records events when the target is visible; fresh sight
                // then takes priority over the last heard position.
                heardNoise = hearNoises(map, npc, *player, noises);
                sawTarget = observeTarget(map, npc, *player);
            }
            if (!heardNoise && !sawTarget)
            {
                decayTargetMemory(world, brain, deltaTime);
            }
        }
    }
}
