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

namespace
{
    bool overlaps(const simple_platformer::Aabb& first, const simple_platformer::Aabb& second)
    {
        return first.position.x < second.position.x + second.size.x &&
               first.position.x + first.size.x > second.position.x &&
               first.position.y < second.position.y + second.size.y &&
               first.position.y + first.size.y > second.position.y;
    }

    void changeState(simple_platformer::NpcBrain& brain, simple_platformer::NpcState state)
    {
        brain.state = state;
        brain.stateTime = 0.0F;
    }

    void faceToward(simple_platformer::Actor& actor, glm::vec2 targetFeet)
    {
        if (targetFeet.x < simple_platformer::feetOf(actor.body.bounds).x)
        {
            actor.facing = simple_platformer::Facing::Left;
        }
        else if (targetFeet.x > simple_platformer::feetOf(actor.body.bounds).x)
        {
            actor.facing = simple_platformer::Facing::Right;
        }
    }

    bool targetIsInBiteRange(
        const simple_platformer::Actor& actor,
        const simple_platformer::Actor& target)
    {
        if (!actor.bite.has_value())
        {
            return false;
        }
        return overlaps(
            simple_platformer::biteHitbox(actor.body.bounds, *actor.bite, actor.facing),
            target.body.bounds);
    }

    glm::vec2 patrolDestination(const simple_platformer::Patrol& patrol)
    {
        return patrol.headingToSecond ? patrol.secondFeet : patrol.firstFeet;
    }

    void requestPath(
        const simple_platformer::TileMap& map,
        const simple_platformer::Actor& actor,
        simple_platformer::PathFollower& follower,
        glm::vec2 goalFeet)
    {
        const simple_platformer::GridPosition start =
            simple_platformer::navigationCell(simple_platformer::feetOf(actor.body.bounds));
        const simple_platformer::GridPosition goal = simple_platformer::navigationCell(goalFeet);
        const bool destinationChanged =
            !follower.destination.has_value() || follower.destination.value_or(goal) != goal;
        const bool displacedAfterCompletion =
            simple_platformer::pathComplete(follower) && start != goal;
        if (!destinationChanged && follower.path.has_value() && !displacedAfterCompletion)
        {
            return;
        }
        if (follower.repathRemaining > 0.0F)
        {
            return;
        }

        if (actor.platformerMovement.has_value() && !actor.platformerMovement->grounded)
        {
            return;
        }

        std::optional<simple_platformer::NavigationPath> path;
        if (actor.flyingMovement.has_value())
        {
            path = simple_platformer::findFlyingPath(map, start, goal);
        }
        else if (actor.platformerMovement.has_value())
        {
            path = simple_platformer::findPlatformerPath(
                map, start, goal, actor.body.bounds.size, actor.platformerMovement->config);
        }
        follower.destination = goal;
        follower.repathRemaining = follower.repathCooldown;
        if (path.has_value())
        {
            simple_platformer::setPath(follower, path.value(), goal);
        }
        else
        {
            follower.path.reset();
            follower.nextStep = 0;
            follower.programElapsed = 0.0F;
        }
    }

    void followDestination(
        const simple_platformer::TileMap& map,
        simple_platformer::Actor& actor,
        simple_platformer::PathFollower& follower,
        glm::vec2 destination,
        float deltaTime)
    {
        requestPath(map, actor, follower, destination);
        if (actor.flyingMovement.has_value())
        {
            actor.intentions = simple_platformer::followFlyingPath(
                actor.body.bounds, *actor.flyingMovement, follower, deltaTime);
        }
        else if (actor.platformerMovement.has_value())
        {
            actor.intentions = simple_platformer::followPlatformerPath(
                actor.body, *actor.platformerMovement, follower, deltaTime);
        }
    }

    void enterPatrolOrIdle(
        simple_platformer::NpcBrain& brain,
        simple_platformer::PathFollower& follower,
        bool hasPatrol)
    {
        simple_platformer::clearPath(follower);
        changeState(
            brain,
            hasPatrol ? simple_platformer::NpcState::Patrol : simple_platformer::NpcState::Idle);
    }

    const simple_platformer::Actor* livingTarget(
        const simple_platformer::World& world,
        const simple_platformer::NpcBrain& brain)
    {
        if (!brain.target.has_value())
        {
            return nullptr;
        }
        const simple_platformer::Actor* target =
            world.findActor(brain.target.value_or(simple_platformer::ActorId{}));
        return target != nullptr && target->life == simple_platformer::LifeState::Alive ? target
                                                                                        : nullptr;
    }

    void chooseNpcState(
        simple_platformer::NpcBrain& brain,
        simple_platformer::PathFollower& follower,
        bool hasPatrol,
        const simple_platformer::Actor* target)
    {
        if (brain.state == simple_platformer::NpcState::Bite)
        {
            return;
        }

        if (target != nullptr && brain.state != simple_platformer::NpcState::Chase)
        {
            simple_platformer::clearPath(follower);
            changeState(brain, simple_platformer::NpcState::Chase);
        }
        else if (target == nullptr && brain.state == simple_platformer::NpcState::Chase)
        {
            enterPatrolOrIdle(brain, follower, hasPatrol);
        }
        else if (brain.state == simple_platformer::NpcState::Idle && hasPatrol)
        {
            changeState(brain, simple_platformer::NpcState::Patrol);
        }
    }

    void updateBiteState(
        simple_platformer::Actor& actor,
        simple_platformer::NpcBrain& brain,
        simple_platformer::PathFollower& follower,
        const simple_platformer::BiteAttack& bite,
        const simple_platformer::Actor* target)
    {
        faceToward(actor, brain.lastSeenTargetFeet);
        if (bite.phase != simple_platformer::BitePhase::Ready || brain.stateTime <= 0.0F)
        {
            return;
        }

        if (target == nullptr)
        {
            enterPatrolOrIdle(brain, follower, actor.patrol.has_value());
        }
        else
        {
            changeState(brain, simple_platformer::NpcState::Chase);
        }
    }

    void updatePatrolState(
        const simple_platformer::TileMap& map,
        simple_platformer::Actor& actor,
        simple_platformer::PathFollower& follower,
        float deltaTime)
    {
        if (!actor.patrol.has_value())
        {
            throw std::logic_error("A patrolling NPC is missing its patrol");
        }
        simple_platformer::Patrol& patrol = *actor.patrol;
        followDestination(map, actor, follower, patrolDestination(patrol), deltaTime);
        if (simple_platformer::pathComplete(follower))
        {
            patrol.headingToSecond = !patrol.headingToSecond;
            simple_platformer::clearPath(follower);
        }
    }

    void updateChaseState(
        const simple_platformer::TileMap& map,
        simple_platformer::Actor& actor,
        simple_platformer::NpcBrain& brain,
        simple_platformer::PathFollower& follower,
        const simple_platformer::Actor* target,
        float deltaTime)
    {
        if (target == nullptr)
        {
            return;
        }

        faceToward(actor, brain.lastSeenTargetFeet);
        if (brain.targetVisible && targetIsInBiteRange(actor, *target))
        {
            simple_platformer::clearPath(follower);
            actor.intentions.primaryAttackPressed = true;
            changeState(brain, simple_platformer::NpcState::Bite);
            return;
        }
        if (brain.targetVisible && actor.rangedWeapon.has_value())
        {
            simple_platformer::clearPath(follower);
            actor.intentions.primaryAttackPressed = true;
            return;
        }
        followDestination(map, actor, follower, brain.lastSeenTargetFeet, deltaTime);
    }

    void updateNpcState(
        const simple_platformer::TileMap& map,
        simple_platformer::World& world,
        simple_platformer::Actor& actor,
        float deltaTime)
    {
        if (!actor.brain.has_value() || !actor.pathFollower.has_value())
        {
            throw std::logic_error("An NPC is missing behaviour components");
        }
        simple_platformer::NpcBrain& brain = *actor.brain;
        simple_platformer::PathFollower& follower = *actor.pathFollower;
        const simple_platformer::Actor* target = livingTarget(world, brain);
        chooseNpcState(brain, follower, actor.patrol.has_value(), target);

        switch (brain.state)
        {
        case simple_platformer::NpcState::Idle:
            break;
        case simple_platformer::NpcState::Patrol:
            updatePatrolState(map, actor, follower, deltaTime);
            break;
        case simple_platformer::NpcState::Chase:
            updateChaseState(map, actor, brain, follower, target, deltaTime);
            break;
        case simple_platformer::NpcState::Bite:
            if (!actor.bite.has_value())
            {
                throw std::logic_error("An NPC in the Bite state is missing its bite attack");
            }
            updateBiteState(actor, brain, follower, *actor.bite, target);
            break;
        }
    }
}

namespace simple_platformer
{
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
                brain.stateTime += deltaTime;
            }
        }
    }
}
