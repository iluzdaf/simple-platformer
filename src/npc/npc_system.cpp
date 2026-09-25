#include "simple_platformer/npc/npc_system.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
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
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/npc/npc_transitions.hpp"
#include "simple_platformer/timing/stopwatch.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    namespace
    {
        // Everything one NPC update needs, passed as one so the state functions carry
        // nothing they do not use themselves.
        struct NpcUpdate
        {
            const TileMap& map;
            World& world;
            float deltaTime;
            NpcBehaviourCost& cost;
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

        void followDestination(
            const NpcUpdate& update,
            Actor& actor,
            PathFollower& follower,
            glm::vec2 destinationFeet)
        {
            requestPath(update, actor, follower, destinationFeet);
            const int tileSize = update.map.tileSize();
            if (actor.flyingMovement.has_value())
            {
                actor.intentions = followFlyingPath(
                    tileSize, actor.body.bounds, *actor.flyingMovement, follower, update.deltaTime);
            }
            else if (actor.platformerMovement.has_value())
            {
                actor.intentions = followPlatformerPath(
                    tileSize, actor.body, *actor.platformerMovement, follower, update.deltaTime);
            }
        }

        NpcFacts gatherNpcFacts(const Actor& actor, const NpcBrain& brain, const Actor* target)
        {
            NpcFacts facts;
            facts.targetKnown = target != nullptr;
            facts.targetVisible = brain.targetVisible;
            facts.targetInBiteRange =
                target != nullptr && brain.targetVisible && targetIsInBiteRange(actor, *target);
            facts.biteReady = actor.bite.has_value() && actor.bite->phase == BitePhase::Ready;
            facts.canShootTarget =
                target != nullptr && brain.targetVisible && actor.rangedWeapon.has_value();
            facts.hasPatrol = actor.patrol.has_value();
            facts.stateElapsed = brain.stateElapsed;
            return facts;
        }

        // Every change drops the path, since the new state chooses its own destination,
        // and a bite is asked for once, as its state is entered.
        void enterNpcState(Actor& actor, NpcBrain& brain, PathFollower& follower, NpcState state)
        {
            clearPath(follower);
            brain.state = state;
            brain.stateElapsed = 0.0F;
            if (state == NpcState::Bite)
            {
                actor.intentions.primaryAttackPressed = true;
            }
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
            glm::vec2 destinationFeet = brain.lastSeenTargetFeet;
            if (actor.platformerMovement.has_value())
            {
                const std::optional<GridPosition> chaseCell = findPlatformerChaseCell(
                    update.map, brain.lastSeenTargetFeet, actor.body.bounds.size);
                if (!chaseCell.has_value())
                {
                    clearPath(follower);
                    return;
                }
                destinationFeet = feetInCell(update.map.tileSize(), chaseCell.value());
            }
            followDestination(update, actor, follower, destinationFeet);
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
            const NpcFacts facts = gatherNpcFacts(actor, brain, target);
            if (const std::optional<NpcState> next = nextNpcState(brain.state, facts))
            {
                enterNpcState(actor, brain, follower, *next);
            }

            switch (brain.state)
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
            }
        }
    }

    NpcBehaviourCost updateNpcBehaviour(const TileMap& map, World& world, float deltaTime)
    {
        requireSeconds(deltaTime, "NPC behaviour time step");
        NpcBehaviourCost cost;
        const NpcUpdate update{map, world, deltaTime, cost};

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
                brain.stateElapsed += deltaTime;
            }
        }
        return cost;
    }
}
