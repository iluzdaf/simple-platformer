#include "simple_platformer/navigation/path_follower.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/input/input_program.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/traversal.hpp"
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

        // -1, 0 or 1: the horizontal input that moves from one position towards the other.
        float directionTowards(float from, float to)
        {
            if (to < from)
            {
                return -1.0F;
            }
            return to > from ? 1.0F : 0.0F;
        }

        // Within the arrival distance on each axis separately, a square rather than a circle.
        bool arrivedAt(glm::vec2 feet, glm::vec2 point)
        {
            return std::abs(point.x - feet.x) <= ArrivalDistance &&
                   std::abs(point.y - feet.y) <= ArrivalDistance;
        }

        // Standing still on the ground at the point. A walk ends this way, and a jump or
        // fall must start this way, because its inputs were recorded from a standing start.
        bool stoppedAt(const Body& body, const PlatformerMovement& movement, glm::vec2 point)
        {
            return movement.grounded && arrivedAt(feetOf(body.bounds), point) &&
                   std::abs(body.velocity.x) <= StoppedSpeed;
        }

        // Walks towards the takeoff and lets go early enough to brake to a stop within
        // arrival distance, so the actor arrives stopped instead of overshooting. Nothing
        // in the air or on another row, where walking would not help.
        InputIntentions approachAndBrake(
            const Body& body,
            const PlatformerMovement& movement,
            glm::vec2 takeoff)
        {
            InputIntentions intentions;
            if (!movement.grounded)
            {
                return intentions;
            }

            const glm::vec2 feet = feetOf(body.bounds);
            const float horizontalOffset = takeoff.x - feet.x;
            const float verticalOffset = takeoff.y - feet.y;
            if (std::abs(verticalOffset) > ArrivalDistance ||
                std::abs(horizontalOffset) <= ArrivalDistance)
            {
                return intentions;
            }

            // With no input, the actor slides v² / 2a before stopping. Hold the direction
            // only while that slide would still stop short of the takeoff.
            const float direction = directionTowards(feet.x, takeoff.x);
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

        // Moves along the held surface towards the climb's start. Returns no intentions
        // once close enough, or when speed or deltaTime allows no movement.
        std::optional<InputIntentions> climbTowards(
            const Body& body,
            const SurfaceClimb& climb,
            glm::vec2 start,
            float deltaTime)
        {
            const glm::vec2 offset = start - feetOf(body.bounds);
            const float maximumStep = climb.config.speed * deltaTime;
            const bool alongCeiling = climb.surface == ClimbSurface::Ceiling;
            const float remaining = alongCeiling ? offset.x : offset.y;
            if (std::abs(remaining) <= FlyingArrivalDistance || maximumStep == 0.0F)
            {
                return std::nullopt;
            }
            InputIntentions approach;
            approach.climbGrip = ClimbGrip::Hold;
            // Full speed until the start is within a tick's travel, then only that fraction
            // of it, so the last tick stops on the start instead of passing it.
            const float direction = std::clamp(remaining / maximumStep, -1.0F, 1.0F);
            (alongCeiling ? approach.direction.x : approach.direction.y) = direction;
            return approach;
        }

        // What following one step gives this tick: the intentions to move with, or that the
        // step is done, so the follower can go straight on to the next one.
        struct StepProgress
        {
            bool complete = false;
            InputIntentions intentions;
        };

        // Walks to the waypoint and is done once standing still on it.
        StepProgress followWalkStep(
            const Body& body,
            const PlatformerMovement& movement,
            const Waypoint& waypoint)
        {
            if (stoppedAt(body, movement, waypoint.feet))
            {
                return {true, {}};
            }
            return {false, approachAndBrake(body, movement, waypoint.feet)};
        }

        // Plays this tick's inputs before advancing time. Arrival is checked on the
        // next tick, after movement has applied the final recorded inputs.
        InputIntentions replayStepInputs(
            PathFollower& follower,
            const InputProgram& inputs,
            float deltaTime)
        {
            const InputIntentions intentions = replayInput(inputs, follower.programElapsed);
            const float duration = durationOf(inputs);
            follower.programElapsed = std::min(duration, follower.programElapsed + deltaTime);
            if (follower.programElapsed >= duration)
            {
                follower.phase = PathStepPhase::AwaitArrival;
            }
            return intentions;
        }

        // Approach a stationary takeoff, replay the recorded inputs, then wait to land.
        StepProgress followAirborneStep(
            const Body& body,
            const PlatformerMovement& movement,
            PathFollower& follower,
            const Waypoint& waypoint,
            glm::vec2 takeoff,
            float deltaTime)
        {
            if (waypoint.inputs.empty())
            {
                throw std::invalid_argument("Jump and fall path steps require an input program");
            }
            if (follower.phase == PathStepPhase::ApproachStart)
            {
                if (!stoppedAt(body, movement, takeoff))
                {
                    return {false, approachAndBrake(body, movement, takeoff)};
                }
                follower.phase = PathStepPhase::ReplayInputs;
            }
            if (follower.phase == PathStepPhase::ReplayInputs)
            {
                return {false, replayStepInputs(follower, waypoint.inputs, deltaTime)};
            }
            // After the program runs out, wait without input until the actor lands and
            // stops. A landing may stop short of the waypoint along its row; the next
            // step starts by walking there.
            if (!movement.grounded || std::abs(body.velocity.x) > StoppedSpeed)
            {
                return {};
            }
            if (std::abs(feetOf(body.bounds).y - waypoint.feet.y) > ArrivalDistance)
            {
                // Landed on another row; the NPC plans again.
                clearPath(follower);
                return {};
            }
            return {true, {}};
        }

        // Reach the recorded starting point before replay. A climber already attached
        // moves along its surface; a grounded one walks and stops; an airborne one grabs.
        StepProgress followClimbStep(
            const Body& body,
            const PlatformerMovement& movement,
            const SurfaceClimb& climb,
            PathFollower& follower,
            const Waypoint& waypoint,
            glm::vec2 start,
            float deltaTime)
        {
            if (waypoint.inputs.empty())
            {
                throw std::invalid_argument("Climb path steps require an input program");
            }
            // Each approach branch reaches the recorded start before replay begins.
            if (follower.phase == PathStepPhase::ApproachStart)
            {
                if (climb.surface != ClimbSurface::None)
                {
                    if (const std::optional<InputIntentions> approach =
                            climbTowards(body, climb, start, deltaTime))
                    {
                        return {false, *approach};
                    }
                }
                else if (!movement.grounded)
                {
                    InputIntentions grab;
                    grab.climbGrip = ClimbGrip::Hold;
                    return {false, grab};
                }
                else if (!stoppedAt(body, movement, start))
                {
                    return {false, approachAndBrake(body, movement, start)};
                }
                else if (
                    waypoint.feet == start &&
                    waypoint.inputs.front().intentions.climbGrip == ClimbGrip::Hold)
                {
                    // Walking may stop within tolerance without touching the wall.
                    // Keep asking for the grab until attached, then replay the climb.
                    return {false, waypoint.inputs.front().intentions};
                }
                follower.phase = PathStepPhase::ReplayInputs;
            }

            if (follower.phase == PathStepPhase::ReplayInputs)
            {
                return {false, replayStepInputs(follower, waypoint.inputs, deltaTime)};
            }
            if (arrivedAt(feetOf(body.bounds), waypoint.feet))
            {
                return {true, {}};
            }
            // The climb ended somewhere else; the NPC plans again. A climber still on a
            // surface keeps its grip, since the intentions leave it as it is.
            clearPath(follower);
            return {};
        }

        // Where the step at this index starts: the previous waypoint, or the path's start.
        glm::vec2 stepStart(const NavigationPath& path, std::size_t index)
        {
            return index == 0 ? path.startFeet : path.waypoints[index - 1].feet;
        }
    }

    void setPath(PathFollower& follower, NavigationPath path)
    {
        follower.path = std::move(path);
        follower.nextStep = 0;
        follower.phase = PathStepPhase::ApproachStart;
        follower.programElapsed = 0.0F;
    }

    void clearPath(PathFollower& follower)
    {
        follower.path.reset();
        follower.nextStep = 0;
        follower.phase = PathStepPhase::ApproachStart;
        follower.programElapsed = 0.0F;
        follower.goal.reset();
    }

    bool pathComplete(const PathFollower& follower)
    {
        return follower.path.has_value() && follower.nextStep >= follower.path->waypoints.size();
    }

    InputIntentions followFlyingPath(
        const Aabb& bounds,
        const FlyingMovement& movement,
        PathFollower& follower,
        float deltaTime)
    {
        requireSeconds(deltaTime, "Flying path following time step");
        if (!isFiniteNonNegative(movement.speed))
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
        while (follower.nextStep < follower.path->waypoints.size())
        {
            const Waypoint& waypoint = follower.path->waypoints[follower.nextStep];
            if (waypoint.traversal != Traversal::Fly)
            {
                throw std::invalid_argument("A flying actor requires flying path steps");
            }
            const glm::vec2 offset = waypoint.feet - feet;
            const float distance = glm::length(offset);
            if (distance > FlyingArrivalDistance)
            {
                // Straight at the waypoint, and no further than it this tick.
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
        const Body& body,
        const PlatformerMovement& movement,
        PathFollower& follower,
        float deltaTime,
        const SurfaceClimb* climb)
    {
        requireSeconds(deltaTime, "Platformer path following time step");
        if (!follower.path.has_value())
        {
            return {};
        }

        // Skip completed steps in the same tick; return intentions for the first unfinished
        // one. A failed step clears the path and returns complete == false, so this loop
        // must return before reading the cleared path or its waypoint again.
        while (follower.nextStep < follower.path->waypoints.size())
        {
            const Waypoint& waypoint = follower.path->waypoints[follower.nextStep];
            const glm::vec2 start = stepStart(*follower.path, follower.nextStep);
            StepProgress progress;
            switch (waypoint.traversal)
            {
            case Traversal::Fly:
                throw std::invalid_argument("A platformer actor cannot follow a flying path step");
            case Traversal::Walk:
                progress = followWalkStep(body, movement, waypoint);
                break;
            case Traversal::Climb:
                if (climb == nullptr)
                {
                    throw std::invalid_argument("A climb path requires a climbing actor");
                }
                progress =
                    followClimbStep(body, movement, *climb, follower, waypoint, start, deltaTime);
                break;
            case Traversal::Fall:
            case Traversal::Jump:
                progress = followAirborneStep(body, movement, follower, waypoint, start, deltaTime);
                break;
            }
            if (!progress.complete)
            {
                return progress.intentions;
            }
            ++follower.nextStep;
            follower.phase = PathStepPhase::ApproachStart;
            follower.programElapsed = 0.0F;
        }
        return {};
    }
}
