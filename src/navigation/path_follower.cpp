#include "simple_platformer/navigation/path_follower.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/input/input_program.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/physics/body.hpp"

namespace simple_platformer
{
    namespace
    {
        // Platformers accept a pixel of feet-position error. Flyers use a tighter
        // tolerance and shorten their final movement to land on the cell's feet point.
        constexpr float ArrivalDistance = 1.0F;
        constexpr float FlyingArrivalDistance = 0.001F;
        // In pixels per second: residual horizontal speed below this counts as stopped.
        constexpr float StoppedSpeed = 0.001F;

        float directionTowards(float from, float to)
        {
            if (to < from)
            {
                return -1.0F;
            }
            return to > from ? 1.0F : 0.0F;
        }

        bool arrivedAt(
            int tileSize,
            const Body& body,
            const PlatformerMovement& movement,
            GridPosition destinationCell)
        {
            const glm::vec2 target = feetInCell(tileSize, destinationCell);
            const glm::vec2 feet = feetOf(body.bounds);
            return movement.grounded && std::abs(target.x - feet.x) <= ArrivalDistance &&
                   std::abs(target.y - feet.y) <= ArrivalDistance;
        }

        bool stoppedAtCell(
            int tileSize,
            const Body& body,
            const PlatformerMovement& movement,
            GridPosition cell)
        {
            return arrivedAt(tileSize, body, movement, cell) &&
                   std::abs(body.velocity.x) <= StoppedSpeed;
        }

        // Walks towards the takeoff cell and lets go early enough to brake to a stop within
        // arrival distance, so the actor arrives stopped instead of overshooting. Nothing
        // in the air or on another row, where walking would not help.
        InputIntentions approachAndBrake(
            int tileSize,
            const Body& body,
            const PlatformerMovement& movement,
            GridPosition takeoff)
        {
            InputIntentions intentions;
            if (!movement.grounded)
            {
                return intentions;
            }

            const glm::vec2 target = feetInCell(tileSize, takeoff);
            const glm::vec2 feet = feetOf(body.bounds);
            const float horizontalOffset = target.x - feet.x;
            const float verticalOffset = target.y - feet.y;
            if (std::abs(verticalOffset) > ArrivalDistance ||
                std::abs(horizontalOffset) <= ArrivalDistance)
            {
                return intentions;
            }

            const float direction = directionTowards(feet.x, target.x);
            const float speedTowardTarget = body.velocity.x * direction;
            const float deceleration = movement.config.groundDeceleration;
            const float brakingDistance =
                deceleration > 0.0F && speedTowardTarget > 0.0F
                    ? speedTowardTarget * speedTowardTarget / (2.0F * deceleration)
                    : 0.0F;
            if (brakingDistance < std::abs(horizontalOffset) - ArrivalDistance)
            {
                intentions.direction.x = direction;
            }
            return intentions;
        }

        struct StepProgress
        {
            bool complete = false;
            InputIntentions intentions;
        };

        StepProgress followWalkStep(
            int tileSize,
            const Body& body,
            const PlatformerMovement& movement,
            GridPosition destination)
        {
            if (stoppedAtCell(tileSize, body, movement, destination))
            {
                return {true, {}};
            }
            return {false, approachAndBrake(tileSize, body, movement, destination)};
        }

        StepProgress followAirborneStep(
            int tileSize,
            const Body& body,
            const PlatformerMovement& movement,
            PathFollower& follower,
            const NavigationStep& step,
            GridPosition takeoff,
            float deltaTime)
        {
            if (step.inputs.empty())
            {
                throw std::invalid_argument("Jump and fall path steps require an input program");
            }
            // The recorded inputs assume a stationary takeoff at the previous cell.
            if (follower.programElapsed == 0.0F &&
                !stoppedAtCell(tileSize, body, movement, takeoff))
            {
                return {false, approachAndBrake(tileSize, body, movement, takeoff)};
            }

            const float programDuration = durationOf(step.inputs);
            if (follower.programElapsed < programDuration)
            {
                const InputIntentions intentions =
                    replayInput(step.inputs, follower.programElapsed);
                follower.programElapsed =
                    std::min(programDuration, follower.programElapsed + deltaTime);
                return {false, intentions};
            }
            // After the program runs out, wait without input until the actor lands in
            // the destination cell.
            if (movement.grounded &&
                cellAtFeet(tileSize, feetOf(body.bounds)) == step.destinationCell)
            {
                follower.programElapsed = 0.0F;
                return {true, {}};
            }
            return {};
        }
    }

    void setPath(PathFollower& follower, NavigationPath path, GridPosition destinationCell)
    {
        if ((!path.steps.empty() && path.steps.back().destinationCell != destinationCell) ||
            (path.steps.empty() && path.start != destinationCell))
        {
            throw std::invalid_argument("A navigation path does not reach its destination");
        }
        follower.path = std::move(path);
        follower.nextStep = 0;
        follower.programElapsed = 0.0F;
        follower.destinationCell = destinationCell;
    }

    void clearPath(PathFollower& follower)
    {
        follower.path.reset();
        follower.nextStep = 0;
        follower.programElapsed = 0.0F;
        follower.destinationCell.reset();
    }

    bool pathComplete(const PathFollower& follower)
    {
        return follower.path.has_value() && follower.nextStep >= follower.path->steps.size();
    }

    InputIntentions followFlyingPath(
        int tileSize,
        const Aabb& bounds,
        const FlyingMovement& movement,
        PathFollower& follower,
        float deltaTime)
    {
        requireSeconds(deltaTime, "Flying path following time step");
        if (!std::isfinite(movement.speed) || movement.speed < 0.0F)
        {
            throw std::invalid_argument(
                "Flying path following requires a finite, non-negative speed");
        }
        if (!follower.path.has_value())
        {
            return {};
        }
        const float maximumMovement = movement.speed * deltaTime;
        if (!std::isfinite(maximumMovement))
        {
            throw std::invalid_argument("Flying path movement must be finite");
        }
        const glm::vec2 feet = feetOf(bounds);
        while (follower.nextStep < follower.path->steps.size())
        {
            const NavigationStep& step = follower.path->steps[follower.nextStep];
            if (step.traversal != Traversal::Fly)
            {
                throw std::invalid_argument("A flying actor requires flying path steps");
            }
            const glm::vec2 offset = feetInCell(tileSize, step.destinationCell) - feet;
            const float distance = glm::length(offset);
            if (distance > FlyingArrivalDistance)
            {
                // Straight at the cell, and no further than it this tick.
                InputIntentions intentions;
                intentions.direction = maximumMovement > 0.0F && distance <= maximumMovement
                                           ? offset / maximumMovement
                                           : glm::normalize(offset);
                return intentions;
            }
            ++follower.nextStep;
        }
        return {};
    }

    InputIntentions followPlatformerPath(
        int tileSize,
        const Body& body,
        const PlatformerMovement& movement,
        PathFollower& follower,
        float deltaTime)
    {
        requireSeconds(deltaTime, "Platformer path following time step");
        if (!follower.path.has_value())
        {
            return {};
        }

        while (follower.nextStep < follower.path->steps.size())
        {
            const NavigationStep& step = follower.path->steps[follower.nextStep];
            if (step.traversal == Traversal::Fly)
            {
                throw std::invalid_argument("A platformer actor cannot follow a flying path step");
            }

            StepProgress progress;
            if (step.traversal == Traversal::Walk)
            {
                progress = followWalkStep(tileSize, body, movement, step.destinationCell);
            }
            else
            {
                const GridPosition takeoff =
                    follower.nextStep == 0
                        ? follower.path->start
                        : follower.path->steps[follower.nextStep - 1].destinationCell;
                progress = followAirborneStep(
                    tileSize, body, movement, follower, step, takeoff, deltaTime);
            }
            if (!progress.complete)
            {
                return progress.intentions;
            }
            ++follower.nextStep;
        }
        return {};
    }
}
