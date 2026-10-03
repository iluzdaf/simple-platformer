#include "simple_platformer/npc/npc_system.hpp"

#include <optional>
#include <stdexcept>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_behaviour.hpp"
#include "simple_platformer/npc/npc_facts.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/npc/npc_update.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    namespace
    {
        void updateTacticState(
            const NpcUpdate& update,
            Actor& actor,
            NpcBrain& brain,
            PathFollower& follower,
            const Actor* target,
            const NpcFacts& facts)
        {
            if (const std::optional<NpcState> next = nextNpcState(brain.tactic, brain.state, facts))
            {
                enterNpcState(actor, brain, follower, *next);
            }
            updateNpcState(update, actor, brain, follower, target, brain.state, brain.stateElapsed);
        }

        // Which state comes next is decided once, from the facts, before the state acts.
        void updateNpcDecision(const NpcUpdate& update, Actor& actor)
        {
            if (!actor.brain.has_value() || !actor.perception.has_value() ||
                !actor.pathFollower.has_value())
            {
                throw std::logic_error("An NPC is missing behaviour components");
            }
            NpcBrain& brain = *actor.brain;
            const NpcPerception& perception = *actor.perception;
            PathFollower& follower = *actor.pathFollower;
            const Actor* target = livingTarget(update.world, brain);
            if (target != nullptr)
            {
                brain.targetLostElapsed = 0.0F;
            }
            const NpcFacts facts =
                gatherNpcFacts(update.map, actor, brain, perception, target, brain.stateElapsed);
            updateTacticState(update, actor, brain, follower, target, facts);
            if (target == nullptr)
            {
                brain.targetLostElapsed += update.deltaTime;
            }
        }
    }

    void updateNpcBehaviour(const TileMap& map, World& world, float deltaTime)
    {
        requireSeconds(deltaTime, "NPC behaviour time step");
        const NpcUpdate update{map, world, deltaTime};

        for (Actor& actor : world.actors())
        {
            if (!actor.brain.has_value())
            {
                continue;
            }
            if (!actor.pathFollower.has_value())
            {
                throw std::logic_error("An NPC is missing its path follower");
            }

            actor.intentions = {};
            NpcBrain& brain = *actor.brain;
            if (actor.life == LifeState::Alive)
            {
                updateNpcDecision(update, actor);
                brain.stateElapsed += deltaTime;
            }
        }
    }
}
