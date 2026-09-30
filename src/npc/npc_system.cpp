#include "simple_platformer/npc/npc_system.hpp"

#include <optional>
#include <stdexcept>
#include <variant>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/actor_navigation.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/platformer_cells.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_activities.hpp"
#include "simple_platformer/npc/npc_activity.hpp"
#include "simple_platformer/npc/npc_activity_script.hpp"
#include "simple_platformer/npc/npc_facts.hpp"
#include "simple_platformer/npc/npc_navigation.hpp"
#include "simple_platformer/npc/npc_scripted_activity.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/npc/npc_state_machine.hpp"
#include "simple_platformer/npc/npc_transitions.hpp"
#include "simple_platformer/npc/npc_update.hpp"
#include "simple_platformer/timing/frame_profile.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    namespace
    {
        void enterNpcState(Actor& actor, NpcBrain& brain, PathFollower& follower, NpcState state)
        {
            brain.state = state;
            brain.stateElapsed = 0.0F;
            enterBuiltInActivity(actor, follower, state);
        }

        void enterMachineActivity(
            const NpcUpdate& update,
            Actor& actor,
            NpcBrain& brain,
            PathFollower& follower,
            NpcMachine& machine,
            const NpcFacts& facts)
        {
            const NpcActivity& activity = activeNpcMachineState(machine).does;
            if (const auto* builtIn = std::get_if<BuiltInNpcActivity>(&activity))
            {
                enterBuiltInActivity(actor, follower, builtIn->state);
            }
            else
            {
                enterScriptedActivity(
                    update, actor, brain, follower, std::get<LuaNpcActivity>(activity), facts);
            }
            machine.activityEntered = true;
        }

        void exitMachineActivity(
            const NpcUpdate& update,
            Actor& actor,
            const NpcBrain& brain,
            const PathFollower& follower,
            const NpcActivity& activity,
            const NpcFacts& facts)
        {
            if (const auto* scripted = std::get_if<LuaNpcActivity>(&activity))
            {
                exitScriptedActivity(update, actor, brain, follower, *scripted, facts);
            }
        }

        void updateMachineActivity(
            const NpcUpdate& update,
            Actor& actor,
            NpcBrain& brain,
            PathFollower& follower,
            const Actor* target,
            NpcMachine& machine,
            const NpcFacts& facts)
        {
            const NpcActivity& activity = activeNpcMachineState(machine).does;
            if (const auto* builtIn = std::get_if<BuiltInNpcActivity>(&activity))
            {
                updateBuiltInActivity(
                    update, actor, brain, follower, target, builtIn->state, facts.stateElapsed);
                return;
            }
            updateScriptedActivity(
                update, actor, brain, follower, std::get<LuaNpcActivity>(activity), facts);
        }

        void updateMachineState(
            const NpcUpdate& update,
            Actor& actor,
            NpcBrain& brain,
            const NpcPerception& perception,
            PathFollower& follower,
            const Actor* target,
            NpcMachine& machine,
            const NpcFacts& facts)
        {
            const NpcActivity previous = activeNpcMachineState(machine).does;
            const bool fired = advanceNpcMachine(machine, facts, update.deltaTime).has_value();
            if (fired && machine.activityEntered)
            {
                exitMachineActivity(update, actor, brain, follower, previous, facts);
                machine.activityEntered = false;
            }

            NpcFacts activeFacts = facts;
            if (fired)
            {
                activeFacts = gatherNpcFacts(
                    update.map, actor, brain, perception, target, machine.stateElapsed);
            }
            if (!machine.activityEntered)
            {
                enterMachineActivity(update, actor, brain, follower, machine, activeFacts);
            }
            updateMachineActivity(update, actor, brain, follower, target, machine, activeFacts);
        }

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
            updateBuiltInActivity(
                update, actor, brain, follower, target, brain.state, brain.stateElapsed);
        }

        // Which state comes next is decided once, from the facts, before the state acts.
        void updateNpcState(const NpcUpdate& update, Actor& actor)
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
            const float stateElapsed =
                actor.machine.has_value() ? actor.machine->stateElapsed : brain.stateElapsed;
            const NpcFacts facts =
                gatherNpcFacts(update.map, actor, brain, perception, target, stateElapsed);
            if (actor.machine.has_value())
            {
                updateMachineState(
                    update, actor, brain, perception, follower, target, *actor.machine, facts);
            }
            else
            {
                updateTacticState(update, actor, brain, follower, target, facts);
            }
        }
    }

    void updateNpcBehaviour(
        const TileMap& map,
        World& world,
        float deltaTime,
        NpcActivityScripts* scripts,
        FrameProfile* profile)
    {
        requireSeconds(deltaTime, "NPC behaviour time step");
        const NpcUpdate update{map, world, deltaTime, scripts, profile};

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
                updateNpcState(update, actor);
                if (actor.machine.has_value())
                {
                    actor.machine->stateElapsed += deltaTime;
                }
                else
                {
                    brain.stateElapsed += deltaTime;
                }
            }
        }
    }
}
