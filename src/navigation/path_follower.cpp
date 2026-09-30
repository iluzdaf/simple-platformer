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

        // Travels the held surface to the start of a climb: along a ceiling sideways,
        // along a wall up or down. Nothing once there.
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

        // A jump or fall runs in three phases: getting to the takeoff and stopping there,
        // replaying the recorded inputs, then waiting to land. programElapsed says which
        // phase it is in: zero before the replay starts, the program's duration after it.
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
            // The recorded inputs assume a stationary takeoff at the previous waypoint.
            if (follower.programElapsed == 0.0F && !stoppedAt(body, movement, takeoff))
            {
                return {false, approachAndBrake(body, movement, takeoff)};
            }

            // Play the inputs at the current time, then move the time on, so the first
            // tick plays the program's start.
            const float programDuration = durationOf(waypoint.inputs);
            if (follower.programElapsed < programDuration)
            {
                const InputIntentions intentions =
                    replayInput(waypoint.inputs, follower.programElapsed);
                follower.programElapsed =
                    std::min(programDuration, follower.programElapsed + deltaTime);
                return {false, intentions};
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
            // Ready for the next step, which starts from its own approach.
            follower.programElapsed = 0.0F;
            return {true, {}};
        }

        // The recorded inputs start where the climb starts: a climber already holding a
        // surface travels along it there, one standing walks there and stops, and one
        // in the air grabs whatever it touches.
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
            // Zero elapsed means the replay has not started, so the climber is still
            // getting to the start. Each branch falls through to the replay once there.
            if (follower.programElapsed == 0.0F)
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
            }

            const float duration = durationOf(waypoint.inputs);
            if (follower.programElapsed < duration)
            {
                const InputIntentions intentions =
                    replayInput(waypoint.inputs, follower.programElapsed);
                follower.programElapsed = std::min(duration, follower.programElapsed + deltaTime);
                return {false, intentions};
            }
            if (arrivedAt(feetOf(body.bounds), waypoint.feet))
            {
                follower.programElapsed = 0.0F;
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
        follower.programElapsed = 0.0F;
    }

    void clearPath(PathFollower& follower)
    {
        follower.path.reset();
        follower.nextStep = 0;
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

        // Follow the current step. A step that is already done hands straight on to the
        // next, so several can finish in one tick; the first unfinished one gives the
        // intentions. A step that goes wrong clears the path and reports itself unfinished,
        // so the loop returns before looking at the path again.
        while (follower.nextStep < follower.path->waypoints.size())
        {
            const Waypoint& waypoint = follower.path->waypoints[follower.nextStep];
            // Where this step's recorded inputs, if any, were recorded from.
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
        }
        return {};
    }
}
