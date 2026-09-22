#include "simple_platformer/npc/npc_system.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/flying_navigation.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    namespace
    {
        void changeState(NpcBrain& brain, NpcState state)
        {
            brain.state = state;
            brain.stateElapsed = 0.0F;
        }

        void faceToward(Actor& actor, glm::vec2 targetFeet)
        {
            if (targetFeet.x < feetOf(actor.body.bounds).x)
            {
                actor.facing = Facing::Left;
            }
            else if (targetFeet.x > feetOf(actor.body.bounds).x)
            {
                actor.facing = Facing::Right;
            }
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

        void requestPath(
            const TileMap& map,
            const Actor& actor,
            PathFollower& follower,
            glm::vec2 goalFeet)
        {
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
            const bool destinationChanged = !follower.destinationCell.has_value() ||
                                            follower.destinationCell.value_or(goal) != goal;
            const bool displacedAfterCompletion = pathComplete(follower) && start != goal;
            if (!destinationChanged && follower.path.has_value() && !displacedAfterCompletion)
            {
                return;
            }
            if (follower.repathRemaining > 0.0F)
            {
                return;
            }

            std::optional<NavigationPath> path;
            if (actor.flyingMovement.has_value())
            {
                path = findFlyingPath(map, start, goal);
            }
            else if (actor.platformerMovement.has_value())
            {
                path = findPlatformerPath(
                    map, start, goal, actor.body.bounds.size, actor.platformerMovement->config);
            }
            follower.destinationCell = goal;
            follower.repathRemaining = follower.repathCooldown;
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
            const TileMap& map,
            Actor& actor,
            PathFollower& follower,
            glm::vec2 destinationFeet,
            float deltaTime)
        {
            requestPath(map, actor, follower, destinationFeet);
            if (actor.flyingMovement.has_value())
            {
                actor.intentions = followFlyingPath(
                    map.tileSize(), actor.body.bounds, *actor.flyingMovement, follower, deltaTime);
            }
            else if (actor.platformerMovement.has_value())
            {
                actor.intentions = followPlatformerPath(
                    map.tileSize(), actor.body, *actor.platformerMovement, follower, deltaTime);
            }
        }

        void enterPatrolOrIdle(NpcBrain& brain, PathFollower& follower, bool hasPatrol)
        {
            clearPath(follower);
            changeState(brain, hasPatrol ? NpcState::Patrol : NpcState::Idle);
        }

        const Actor* livingTarget(const World& world, const NpcBrain& brain)
        {
            if (!brain.target.has_value())
            {
                return nullptr;
            }
            const Actor* target = world.findActor(brain.target.value_or(ActorId{}));
            return target != nullptr && target->life == LifeState::Alive ? target : nullptr;
        }

        void chooseNpcState(
            NpcBrain& brain,
            PathFollower& follower,
            bool hasPatrol,
            const Actor* target)
        {
            if (brain.state == NpcState::Bite)
            {
                return;
            }

            if (target != nullptr && brain.state != NpcState::Chase)
            {
                clearPath(follower);
                changeState(brain, NpcState::Chase);
            }
            else if (target == nullptr && brain.state == NpcState::Chase)
            {
                enterPatrolOrIdle(brain, follower, hasPatrol);
            }
            else if (brain.state == NpcState::Idle && hasPatrol)
            {
                changeState(brain, NpcState::Patrol);
            }
        }

        void updateBiteState(
            Actor& actor,
            NpcBrain& brain,
            PathFollower& follower,
            const BiteAttack& bite,
            const Actor* target)
        {
            faceToward(actor, brain.lastSeenTargetFeet);
            if (bite.phase != BitePhase::Ready || brain.stateElapsed <= 0.0F)
            {
                return;
            }

            if (target == nullptr)
            {
                enterPatrolOrIdle(brain, follower, actor.patrol.has_value());
            }
            else
            {
                changeState(brain, NpcState::Chase);
            }
        }

        void updatePatrolState(
            const TileMap& map,
            Actor& actor,
            PathFollower& follower,
            float deltaTime)
        {
            if (!actor.patrol.has_value())
            {
                throw std::logic_error("A patrolling NPC is missing its patrol");
            }
            Patrol& patrol = *actor.patrol;
            followDestination(map, actor, follower, patrolDestination(patrol), deltaTime);
            if (pathComplete(follower))
            {
                patrol.headingToSecond = !patrol.headingToSecond;
                clearPath(follower);
            }
        }

        void updateChaseState(
            const TileMap& map,
            Actor& actor,
            NpcBrain& brain,
            PathFollower& follower,
            const Actor* target,
            float deltaTime)
        {
            if (target == nullptr)
            {
                return;
            }

            faceToward(actor, brain.lastSeenTargetFeet);
            if (brain.targetVisible && targetIsInBiteRange(actor, *target))
            {
                clearPath(follower);
                actor.intentions.primaryAttackPressed = true;
                changeState(brain, NpcState::Bite);
                return;
            }
            if (brain.targetVisible && actor.rangedWeapon.has_value())
            {
                clearPath(follower);
                actor.intentions.aimDirection =
                    centerOf(target->body.bounds) - centerOf(actor.body.bounds);
                actor.intentions.primaryAttackPressed = true;
                return;
            }
            glm::vec2 destinationFeet = brain.lastSeenTargetFeet;
            if (actor.platformerMovement.has_value())
            {
                const std::optional<GridPosition> chaseCell =
                    findPlatformerChaseCell(map, brain.lastSeenTargetFeet, actor.body.bounds.size);
                if (!chaseCell.has_value())
                {
                    clearPath(follower);
                    return;
                }
                destinationFeet = feetInCell(map.tileSize(), chaseCell.value());
            }
            followDestination(map, actor, follower, destinationFeet, deltaTime);
        }

        void updateNpcState(const TileMap& map, World& world, Actor& actor, float deltaTime)
        {
            if (!actor.brain.has_value() || !actor.pathFollower.has_value())
            {
                throw std::logic_error("An NPC is missing behaviour components");
            }
            NpcBrain& brain = *actor.brain;
            PathFollower& follower = *actor.pathFollower;
            const Actor* target = livingTarget(world, brain);
            chooseNpcState(brain, follower, actor.patrol.has_value(), target);

            switch (brain.state)
            {
            case NpcState::Idle:
                break;
            case NpcState::Patrol:
                updatePatrolState(map, actor, follower, deltaTime);
                break;
            case NpcState::Chase:
                updateChaseState(map, actor, brain, follower, target, deltaTime);
                break;
            case NpcState::Bite:
                if (!actor.bite.has_value())
                {
                    throw std::logic_error("An NPC in the Bite state is missing its bite attack");
                }
                updateBiteState(actor, brain, follower, *actor.bite, target);
                break;
            }
        }
    }

    void updateNpcBehaviour(const TileMap& map, World& world, float deltaTime)
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0F)
        {
            throw std::invalid_argument("NPC behaviour time must be finite and non-negative");
        }

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
                updateNpcState(map, world, actor, deltaTime);
                brain.stateElapsed += deltaTime;
            }
        }
    }
}
