#include "simple_platformer/npc/npc_system.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <variant>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/flying_navigation.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/path_search.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_activity.hpp"
#include "simple_platformer/npc/npc_activity_script.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/npc/npc_state_machine.hpp"
#include "simple_platformer/npc/npc_transitions.hpp"
#include "simple_platformer/timing/stopwatch.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    namespace
    {
        // How often a searching NPC turns to look the other way.
        constexpr float SearchTurnSeconds = 0.5F;

        // Everything one NPC update needs, passed as one so the state functions carry
        // nothing they do not use themselves.
        struct NpcUpdate
        {
            const TileMap& map;
            World& world;
            float deltaTime;
            NpcBehaviourCost& cost;
            NpcActivityScripts* scripts;
        };

        // Looking at the target is an aim, like everything else an actor intends; the
        // movement update turns it into a facing.
        void aimToward(Actor& actor, glm::vec2 targetFeet)
        {
            actor.intentions.aimDirection = targetFeet - feetOf(actor.body.bounds);
        }

        bool targetIsInBiteRange(const Actor& actor, const Actor& target)
        {
            if (!actor.bite.has_value())
            {
                return false;
            }
            return overlaps(
                biteHitbox(actor.body.bounds, *actor.bite, actor.facing), target.body.bounds);
        }

        glm::vec2 patrolDestination(const Patrol& patrol)
        {
            return patrol.headingToSecond ? patrol.secondFeet : patrol.firstFeet;
        }

        // Whether the follower needs a path to this goal: it has none, or one to another
        // cell, or has finished its path and been moved off the goal since.
        bool needsPath(const PathFollower& follower, GridPosition start, GridPosition goal)
        {
            const bool destinationChanged = !follower.destinationCell.has_value() ||
                                            follower.destinationCell.value_or(goal) != goal;
            const bool displacedAfterCompletion = pathComplete(follower) && start != goal;
            return destinationChanged || !follower.path.has_value() || displacedAfterCompletion;
        }

        // Whether a tile has broken since the path was planned. The path may run through
        // it, so the follower plans again at once, cooldown or not.
        bool plannedBeforeABreak(const TileMap& map, const PathFollower& follower)
        {
            return follower.breaksWhenPlanned != map.brokenCells().size();
        }

        // The search simulates at the update's step, which this actor is about to be moved
        // with.
        void requestPath(
            const NpcUpdate& update,
            const Actor& actor,
            PathFollower& follower,
            glm::vec2 goalFeet)
        {
            const TileMap& map = update.map;
            GridPosition start = cellAtFeet(map.tileSize(), feetOf(actor.body.bounds));
            if (actor.platformerMovement.has_value())
            {
                if (!actor.platformerMovement->grounded)
                {
                    return;
                }
                const std::optional<GridPosition> supportedStart =
                    findPlatformerStartCell(map, actor.body.bounds);
                if (!supportedStart.has_value())
                {
                    return;
                }
                start = supportedStart.value_or(start);
            }
            const GridPosition goal = cellAtFeet(map.tileSize(), goalFeet);
            if (!plannedBeforeABreak(map, follower) &&
                (!needsPath(follower, start, goal) || follower.repathRemaining > 0.0F))
            {
                return;
            }

            std::optional<NavigationPath> path;
            PathSearchStatistics statistics;
            const Stopwatch stopwatch;
            if (actor.flyingMovement.has_value())
            {
                path = findFlyingPath(map, start, goal, &statistics);
            }
            else if (actor.platformerMovement.has_value())
            {
                path = findPlatformerPath(
                    map,
                    start,
                    goal,
                    actor.body.bounds.size,
                    actor.platformerMovement->config,
                    update.deltaTime,
                    PlatformerNavigationConfig{},
                    &statistics,
                    &update.world.platformerConnections());
            }
            NpcBehaviourCost& cost = update.cost;
            ++cost.pathSearches;
            cost.searches.nodesExpanded += statistics.nodesExpanded;
            cost.searches.cellsReused += statistics.cellsReused;
            cost.searches.pathsRemembered += statistics.pathsRemembered;
            cost.searches.deferred += statistics.deferred;
            cost.searches.simulatedTicks += statistics.simulatedTicks;
            cost.searchSeconds += stopwatch.elapsedSeconds();
            follower.destinationCell = goal;
            follower.breaksWhenPlanned = map.brokenCells().size();
            // A deferred search is asked again next step, once the fill has caught up.
            follower.repathRemaining = statistics.deferred > 0 ? 0.0F : follower.repathCooldown;
            if (path.has_value())
            {
                setPath(follower, path.value(), goal);
            }
            else
            {
                follower.path.reset();
                follower.nextStep = 0;
                follower.programElapsed = 0.0F;
            }
        }

        InputIntentions intentionsToFollow(
            const NpcUpdate& update,
            Actor& actor,
            PathFollower& follower,
            glm::vec2 destinationFeet)
        {
            requestPath(update, actor, follower, destinationFeet);
            const int tileSize = update.map.tileSize();
            if (actor.flyingMovement.has_value())
            {
                return followFlyingPath(
                    tileSize, actor.body.bounds, *actor.flyingMovement, follower, update.deltaTime);
            }
            if (actor.platformerMovement.has_value())
            {
                return followPlatformerPath(
                    tileSize, actor.body, *actor.platformerMovement, follower, update.deltaTime);
            }
            return {};
        }

        void followDestination(
            const NpcUpdate& update,
            Actor& actor,
            PathFollower& follower,
            glm::vec2 destinationFeet)
        {
            actor.intentions = intentionsToFollow(update, actor, follower, destinationFeet);
        }

        NpcFacts gatherNpcFacts(
            const Actor& actor,
            const NpcBrain& brain,
            const Actor* target,
            float stateElapsed)
        {
            NpcFacts facts;
            facts.targetKnown = target != nullptr;
            facts.targetVisible = brain.targetVisible;
            facts.targetInBiteRange =
                target != nullptr && brain.targetVisible && targetIsInBiteRange(actor, *target);
            facts.biteReady = actor.bite.has_value() && actor.bite->phase == BitePhase::Ready;
            facts.targetInSights =
                target != nullptr && brain.targetVisible && actor.rangedWeapon.has_value();
            const float standoffDistance =
                actor.senses.has_value() ? actor.senses->standoffDistance : 0.0F;
            facts.targetTooClose =
                target != nullptr &&
                glm::distance(feetOf(actor.body.bounds), brain.lastSeenTargetFeet) <
                    standoffDistance;
            facts.hasPatrol = actor.patrol.has_value();
            const float searchDuration =
                actor.senses.has_value() ? actor.senses->searchDuration : 0.0F;
            facts.searches = searchDuration > 0.0F;
            facts.searchTimeUp = stateElapsed >= searchDuration;
            facts.stateElapsed = stateElapsed;
            return facts;
        }

        // Every activity change drops the old route, and a bite is asked for once as its
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
            followDestination(update, actor, follower, patrolDestination(patrol));
            if (pathComplete(follower))
            {
                patrol.headingToSecond = !patrol.headingToSecond;
                clearPath(follower);
            }
        }

        // Where the last seen feet send a pursuer. A platformer needs a standable cell
        // near them, and has nowhere to go when there is none.
        std::optional<glm::vec2> lastSeenDestination(
            const NpcUpdate& update,
            const Actor& actor,
            const NpcBrain& brain)
        {
            if (!actor.platformerMovement.has_value())
            {
                return brain.lastSeenTargetFeet;
            }
            const std::optional<GridPosition> chaseCell = findPlatformerChaseCell(
                update.map, brain.lastSeenTargetFeet, actor.body.bounds.size);
            if (!chaseCell.has_value())
            {
                return std::nullopt;
            }
            return feetInCell(update.map.tileSize(), chaseCell.value());
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

            aimToward(actor, brain.lastSeenTargetFeet);
            const std::optional<glm::vec2> destination = lastSeenDestination(update, actor, brain);
            if (!destination.has_value())
            {
                clearPath(follower);
                return;
            }
            followDestination(update, actor, follower, *destination);
        }

        // Looking about is an aim that turns every SearchTurnSeconds, first towards where
        // the target was last seen.
        void lookAbout(Actor& actor, const NpcBrain& brain, float stateElapsed)
        {
            const float toward = brain.lastSeenTargetFeet.x - feetOf(actor.body.bounds).x;
            float side = toward < 0.0F ? -1.0F : 1.0F;
            const int turns = static_cast<int>(stateElapsed / SearchTurnSeconds);
            if (turns % 2 == 1)
            {
                side = -side;
            }
            actor.intentions.aimDirection = {side, 0.0F};
        }

        // A search finishes the walk to where the target was last seen, and looks about
        // once it is there or cannot get there.
        void updateSearchState(
            const NpcUpdate& update,
            Actor& actor,
            const NpcBrain& brain,
            PathFollower& follower,
            float stateElapsed)
        {
            const std::optional<glm::vec2> destination = lastSeenDestination(update, actor, brain);
            if (destination.has_value())
            {
                followDestination(update, actor, follower, *destination);
            }
            else
            {
                clearPath(follower);
            }
            if (!follower.path.has_value() || pathComplete(follower))
            {
                lookAbout(actor, brain, stateElapsed);
            }
        }

        // The target is visible for as long as this state lasts, since the transitions
        // leave it on the update sight is lost, so the aim may read the target's body.
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

        // A retreat backs straight away from where the target was last seen, facing it and
        // firing. A walker holds at a ledge rather than step off it.
        void updateRetreatState(const NpcUpdate& update, Actor& actor, const NpcBrain& brain)
        {
            const glm::vec2 feet = feetOf(actor.body.bounds);
            glm::vec2 away = feet - brain.lastSeenTargetFeet;
            if (actor.platformerMovement.has_value())
            {
                const float side = away.x < 0.0F ? -1.0F : 1.0F;
                away = {side, 0.0F};
                const GridPosition ahead = cellAtFeet(
                    update.map.tileSize(), feet + glm::vec2{side * actor.body.bounds.size.x, 0.0F});
                if (!update.map.contains(ahead) ||
                    !canStandAt(update.map, ahead, actor.body.bounds.size))
                {
                    away = {0.0F, 0.0F};
                }
            }
            actor.intentions.direction = away;
            aimToward(actor, brain.lastSeenTargetFeet);
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
                aimToward(actor, brain.lastSeenTargetFeet);
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
                // A watch looks about from where the NPC stands; its path was cleared on
                // entry and nothing here asks for one.
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
            snapshot.facts = facts;
            snapshot.pathComplete = pathComplete(follower);
            if (facts.targetKnown)
            {
                snapshot.targetFeet = brain.lastSeenTargetFeet;
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
                    intentionsToFollow(update, actor, follower, *command.routeTo);
                actor.intentions.direction = movement.direction;
                actor.intentions.jumpPressed = movement.jumpPressed;
                actor.intentions.jumpHeld = movement.jumpHeld;
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
                activeFacts = gatherNpcFacts(actor, brain, target, machine.stateElapsed);
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
            if (!actor.brain.has_value() || !actor.pathFollower.has_value())
            {
                throw std::logic_error("An NPC is missing behaviour components");
            }
            NpcBrain& brain = *actor.brain;
            PathFollower& follower = *actor.pathFollower;
            const Actor* target = livingTarget(update.world, brain);
            const float stateElapsed =
                actor.machine.has_value() ? actor.machine->stateElapsed : brain.stateElapsed;
            const NpcFacts facts = gatherNpcFacts(actor, brain, target, stateElapsed);
            if (actor.machine.has_value())
            {
                updateMachineState(update, actor, brain, follower, target, *actor.machine, facts);
            }
            else
            {
                updateTacticState(update, actor, brain, follower, target, facts);
            }
        }
    }

    NpcBehaviourCost updateNpcBehaviour(
        const TileMap& map,
        World& world,
        float deltaTime,
        NpcActivityScripts* scripts)
    {
        requireSeconds(deltaTime, "NPC behaviour time step");
        NpcBehaviourCost cost;
        const NpcUpdate update{map, world, deltaTime, cost, scripts};

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
            PathFollower& follower = *actor.pathFollower;
            follower.repathRemaining = std::max(0.0F, follower.repathRemaining - deltaTime);
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
        return cost;
    }

    void forgetNpcActivities(const std::vector<ActorId>& actors, NpcActivityScripts& scripts)
    {
        for (const ActorId actor : actors)
        {
            scripts.forget(actor);
        }
    }
}
