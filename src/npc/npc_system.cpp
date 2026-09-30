#include "simple_platformer/npc/npc_system.hpp"

#include <optional>
#include <stdexcept>
#include <variant>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/actor_navigation.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/platformer_cells.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_activity.hpp"
#include "simple_platformer/npc/npc_activity_script.hpp"
#include "simple_platformer/npc/npc_facts.hpp"
#include "simple_platformer/npc/npc_navigation.hpp"
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
        constexpr float SearchTurnSeconds = 0.5F;

        // Looking at the target is an aim, like everything else an actor intends; the
        // movement update turns it into a facing.
        void aimToward(Actor& actor, glm::vec2 targetFeet)
        {
            actor.intentions.aimDirection = targetFeet - feetOf(actor.body.bounds);
        }

        glm::vec2 patrolGoal(const Patrol& patrol)
        {
            return patrol.headingToSecond ? patrol.secondFeet : patrol.firstFeet;
        }

        // Every activity change drops the old path, and a bite is asked for once as its
        // activity is entered.
        void enterBuiltInActivity(Actor& actor, PathFollower& follower, NpcState state)
        {
            clearPath(follower);
            if (state == NpcState::Bite)
            {
                actor.intentions.primaryAttackPressed = true;
            }
        }

        void enterNpcState(Actor& actor, NpcBrain& brain, PathFollower& follower, NpcState state)
        {
            brain.state = state;
            brain.stateElapsed = 0.0F;
            enterBuiltInActivity(actor, follower, state);
        }

        void updatePatrolState(const NpcUpdate& update, Actor& actor, PathFollower& follower)
        {
            if (!actor.patrol.has_value())
            {
                throw std::logic_error("A patrolling NPC is missing its patrol");
            }
            Patrol& patrol = *actor.patrol;
            actor.intentions = intentionsToReach(update, actor, follower, patrolGoal(patrol));
            if (pathComplete(follower))
            {
                patrol.headingToSecond = !patrol.headingToSecond;
                clearPath(follower);
            }
        }

        void updateChaseState(
            const NpcUpdate& update,
            Actor& actor,
            const NpcBrain& brain,
            PathFollower& follower,
            const Actor* target)
        {
            if (target == nullptr)
            {
                throw std::logic_error("A chasing NPC has no target");
            }

            actor.intentions =
                intentionsToReach(update, actor, follower, brain.lastKnownTargetFeet);
            aimToward(actor, brain.lastKnownTargetFeet);
        }

        // Looking about is an aim that turns every SearchTurnSeconds, first towards where
        // the target was last known to be.
        void lookAbout(Actor& actor, const NpcBrain& brain, float stateElapsed)
        {
            const float toward = brain.lastKnownTargetFeet.x - feetOf(actor.body.bounds).x;
            float side = toward < 0.0F ? -1.0F : 1.0F;
            const int turns = static_cast<int>(stateElapsed / SearchTurnSeconds);
            if (turns % 2 == 1)
            {
                side = -side;
            }
            actor.intentions.aimDirection = {side, 0.0F};
        }

        // A search finishes the walk to where the target was last known to be, and looks about
        // once it is there or cannot get there.
        void updateSearchState(
            const NpcUpdate& update,
            Actor& actor,
            const NpcBrain& brain,
            PathFollower& follower,
            float stateElapsed)
        {
            actor.intentions =
                intentionsToReach(update, actor, follower, brain.lastKnownTargetFeet);
            if (!follower.path.has_value() || pathComplete(follower))
            {
                lookAbout(actor, brain, stateElapsed);
            }
        }

        // The tactic leaves Shoot when sight is lost. A machine using this activity
        // must handle lost sight and lost targets in its own transitions.
        void updateShootState(Actor& actor, const Actor* target)
        {
            if (target == nullptr)
            {
                throw std::logic_error("A shooting NPC has no target");
            }
            actor.intentions.aimDirection =
                centerOf(target->body.bounds) - centerOf(actor.body.bounds);
            actor.intentions.primaryAttackPressed = true;
        }

        // Retreat aims at the last known target feet and requests a primary attack.
        // A walker stops at a ledge instead of stepping off it.
        void updateRetreatState(const NpcUpdate& update, Actor& actor, const NpcBrain& brain)
        {
            const glm::vec2 feet = feetOf(actor.body.bounds);
            glm::vec2 away = feet - brain.lastKnownTargetFeet;
            if (actor.platformerMovement.has_value())
            {
                const float side = away.x < 0.0F ? -1.0F : 1.0F;
                away = {side, 0.0F};
                const Cell ahead = cellAtFeet(
                    update.map.tileSize(), feet + glm::vec2{side * actor.body.bounds.size.x, 0.0F});
                if (!canStandAt(update.map, ahead, actor.body.bounds.size))
                {
                    away = {0.0F, 0.0F};
                }
            }
            actor.intentions.direction = away;
            aimToward(actor, brain.lastKnownTargetFeet);
            actor.intentions.primaryAttackPressed = true;
        }

        void updateBuiltInActivity(
            const NpcUpdate& update,
            Actor& actor,
            NpcBrain& brain,
            PathFollower& follower,
            const Actor* target,
            NpcState state,
            float stateElapsed)
        {
            switch (state)
            {
            case NpcState::Idle:
                break;
            case NpcState::Patrol:
                updatePatrolState(update, actor, follower);
                break;
            case NpcState::Chase:
                updateChaseState(update, actor, brain, follower, target);
                break;
            case NpcState::Bite:
                aimToward(actor, brain.lastKnownTargetFeet);
                break;
            case NpcState::Shoot:
                updateShootState(actor, target);
                break;
            case NpcState::Search:
                updateSearchState(update, actor, brain, follower, stateElapsed);
                break;
            case NpcState::Retreat:
                updateRetreatState(update, actor, brain);
                break;
            case NpcState::Watch:
                lookAbout(actor, brain, stateElapsed);
                break;
            }
        }

        NpcActivitySnapshot activitySnapshot(
            const Actor& actor,
            const NpcBrain& brain,
            const PathFollower& follower,
            const NpcFacts& facts)
        {
            NpcActivitySnapshot snapshot;
            snapshot.feet = feetOf(actor.body.bounds);
            snapshot.patrol = actor.patrol;
            snapshot.facts = facts;
            snapshot.routeComplete = pathComplete(follower);
            if (facts.targetKnown)
            {
                snapshot.targetFeet = brain.lastKnownTargetFeet;
            }
            return snapshot;
        }

        NpcActivityScripts& requiredScripts(const NpcUpdate& update)
        {
            if (update.scripts == nullptr)
            {
                throw std::logic_error("A scripted NPC activity needs the scripting runtime");
            }
            return *update.scripts;
        }

        void applyScriptCommand(
            const NpcUpdate& update,
            Actor& actor,
            PathFollower& follower,
            const NpcActivityCommand& command)
        {
            actor.intentions = command.intentions;
            if (command.clearRoute)
            {
                clearPath(follower);
            }
            if (command.routeTo.has_value())
            {
                const InputIntentions movement =
                    intentionsToReach(update, actor, follower, *command.routeTo);
                actor.intentions.direction = movement.direction;
                actor.intentions.jumpPressed = movement.jumpPressed;
                actor.intentions.jumpHeld = movement.jumpHeld;
                actor.intentions.climbGrip = movement.climbGrip;
            }
            if (command.aimAt.has_value())
            {
                aimToward(actor, *command.aimAt);
            }
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
                clearPath(follower);
                requiredScripts(update).enter(
                    actor.id,
                    std::get<LuaNpcActivity>(activity),
                    activitySnapshot(actor, brain, follower, facts));
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
                requiredScripts(update).exit(
                    actor.id, *scripted, activitySnapshot(actor, brain, follower, facts));
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
            const NpcActivityCommand command = requiredScripts(update).update(
                actor.id,
                std::get<LuaNpcActivity>(activity),
                activitySnapshot(actor, brain, follower, facts),
                update.deltaTime);
            applyScriptCommand(update, actor, follower, command);
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

    void forgetNpcActivities(const std::vector<ActorId>& actors, NpcActivityScripts& scripts)
    {
        for (const ActorId actor : actors)
        {
            scripts.forget(actor);
        }
    }
}
