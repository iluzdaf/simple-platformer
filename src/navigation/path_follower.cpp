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
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/input_program.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/physics/body.hpp"

namespace
{
    constexpr float ArrivalDistance = 1.0F;
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
        const simple_platformer::Body& body,
        const simple_platformer::PlatformerMovement& movement,
        simple_platformer::GridPosition destination)
    {
        const glm::vec2 target = simple_platformer::navigationFeet(destination);
        const glm::vec2 feet = simple_platformer::feetOf(body.bounds);
        return movement.grounded && std::abs(target.x - feet.x) <= ArrivalDistance &&
               std::abs(target.y - feet.y) <= ArrivalDistance;
    }

    bool readyForInputProgram(
        const simple_platformer::Body& body,
        const simple_platformer::PlatformerMovement& movement,
        simple_platformer::GridPosition takeoff)
    {
        return arrivedAt(body, movement, takeoff) && std::abs(body.velocity.x) <= StoppedSpeed;
    }

    simple_platformer::InputIntentions approachAndBrake(
        const simple_platformer::Body& body,
        const simple_platformer::PlatformerMovement& movement,
        simple_platformer::GridPosition takeoff)
    {
        simple_platformer::InputIntentions intentions;
        if (!movement.grounded)
        {
            return intentions;
        }

        const glm::vec2 target = simple_platformer::navigationFeet(takeoff);
        const glm::vec2 feet = simple_platformer::feetOf(body.bounds);
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
}

namespace simple_platformer
{
    GridPosition navigationCell(glm::vec2 feet)
    {
        constexpr float BoundaryOffset = 0.001F;
        return worldToGrid({feet.x, feet.y - BoundaryOffset});
    }

    glm::vec2 navigationFeet(GridPosition cell)
    {
        const glm::vec2 topLeft = gridToWorld(cell);
        return {
            topLeft.x + static_cast<float>(TileSize) * 0.5F,
            topLeft.y + static_cast<float>(TileSize)};
    }

    void setPath(PathFollower& follower, NavigationPath path, GridPosition destination)
    {
        if ((!path.steps.empty() && path.steps.back().destination != destination) ||
            (path.steps.empty() && path.start != destination))
        {
            throw std::invalid_argument("A navigation path does not reach its destination");
        }
        follower.path = std::move(path);
        follower.nextStep = 0;
        follower.programElapsed = 0.0F;
        follower.destination = destination;
    }

    void clearPath(PathFollower& follower)
    {
        follower.path.reset();
        follower.nextStep = 0;
        follower.programElapsed = 0.0F;
        follower.destination.reset();
    }

    bool pathComplete(const PathFollower& follower)
    {
        return follower.path.has_value() && follower.nextStep >= follower.path->steps.size();
    }

    InputIntentions followFlyingPath(const Aabb& bounds, PathFollower& follower)
    {
        if (!follower.path.has_value())
        {
            return {};
        }
        const glm::vec2 feet = feetOf(bounds);
        while (follower.nextStep < follower.path->steps.size())
        {
            const NavigationStep& step = follower.path->steps[follower.nextStep];
            if (step.traversal != Traversal::Fly)
            {
                throw std::invalid_argument("A flying actor requires flying path steps");
            }
            const glm::vec2 offset = navigationFeet(step.destination) - feet;
            if (glm::length(offset) > ArrivalDistance)
            {
                InputIntentions intentions;
                intentions.direction = glm::normalize(offset);
                return intentions;
            }
            ++follower.nextStep;
        }
        return {};
    }

    InputIntentions followPlatformerPath(
        const Body& body,
        const PlatformerMovement& movement,
        PathFollower& follower,
        float deltaTime)
    {
        if (!std::isfinite(deltaTime) || deltaTime <= 0.0F)
        {
            throw std::invalid_argument(
                "Platformer path following requires a positive finite time step");
        }
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

            if (step.traversal == Traversal::Walk)
            {
                if (readyForInputProgram(body, movement, step.destination))
                {
                    ++follower.nextStep;
                    continue;
                }
                return approachAndBrake(body, movement, step.destination);
            }

            if (step.inputs.empty())
            {
                throw std::invalid_argument("Jump and fall path steps require an input program");
            }
            if (follower.programElapsed == 0.0F)
            {
                const GridPosition takeoff =
                    follower.nextStep == 0
                        ? follower.path->start
                        : follower.path->steps[follower.nextStep - 1].destination;
                if (!readyForInputProgram(body, movement, takeoff))
                {
                    return approachAndBrake(body, movement, takeoff);
                }
            }

            const float programDuration = durationOf(step.inputs);
            if (follower.programElapsed < programDuration)
            {
                const InputIntentions intentions =
                    replayInput(step.inputs, follower.programElapsed);
                follower.programElapsed =
                    std::min(programDuration, follower.programElapsed + deltaTime);
                return intentions;
            }
            if (movement.grounded && navigationCell(feetOf(body.bounds)) == step.destination)
            {
                ++follower.nextStep;
                follower.programElapsed = 0.0F;
                continue;
            }
            return {};
        }
        return {};
    }
}
