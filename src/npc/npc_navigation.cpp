#include "simple_platformer/npc/npc_navigation.hpp"

#include <optional>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/navigation/actor_navigation.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc_update.hpp"
#include "simple_platformer/timing/frame_profile.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    namespace
    {
        // In pixels: a goal that moves less than this keeps its path, so a goal that
        // follows a drifting target is not planned for every frame.
        constexpr float ReplanDistance = 8.0F;

        // Whether the follower needs a path to this goal: it has none, or one planned
        // for a goal that has since moved away, or has finished its path and been moved
        // off its end since.
        bool needsPath(const PathFollower& follower, glm::vec2 feet, glm::vec2 goal)
        {
            if (!follower.path.has_value() || !follower.goal.has_value())
            {
                return true;
            }
            if (glm::distance(*follower.goal, goal) > ReplanDistance)
            {
                return true;
            }
            return pathComplete(follower) &&
                   glm::distance(feet, endOf(*follower.path)) > ReplanDistance;
        }

        // Whether a tile has broken since the path was planned. The path may run through
        // it, so the follower plans again at once, whatever the goal.
        bool plannedBeforeABreak(const TileMap& map, const PathFollower& follower)
        {
            return follower.breaksWhenPlanned != map.brokenCells().size();
        }

        // Keeps the follower's path to the goal, or finds a new one when needsPath or a
        // break says to. The search simulates at the update's step, which this actor is
        // about to be moved with.
        void planPathTo(
            const NpcUpdate& update,
            const Actor& actor,
            PathFollower& follower,
            glm::vec2 goalFeet)
        {
            const TileMap& map = update.map;
            if (!plannedBeforeABreak(map, follower) &&
                !needsPath(follower, feetOf(actor.body.bounds), goalFeet))
            {
                return;
            }

            std::optional<NavigationPathResult> pathResult;
            {
                const PhaseScope searchPhase(update.profile, "Navigation", "Path search");
                pathResult = findActorPath(
                    map,
                    actor,
                    goalFeet,
                    update.deltaTime,
                    update.world.platformerConnections(),
                    update.profile);
            }
            if (!pathResult.has_value())
            {
                return;
            }
            follower.goal = goalFeet;
            follower.breaksWhenPlanned = map.brokenCells().size();
            if (pathResult->path.has_value())
            {
                setPath(follower, std::move(pathResult->path.value()));
            }
            else
            {
                // A deferred search has no path, so the next step plans again once the
                // fill has caught up.
                follower.path.reset();
                follower.nextStep = 0;
                follower.programElapsed = 0.0F;
            }
        }
    }

    InputIntentions intentionsToReach(
        const NpcUpdate& update,
        Actor& actor,
        PathFollower& follower,
        glm::vec2 goalFeet)
    {
        planPathTo(update, actor, follower, goalFeet);
        if (actor.flyingMovement.has_value())
        {
            return followFlyingPath(
                actor.body.bounds, *actor.flyingMovement, follower, update.deltaTime);
        }
        if (actor.platformerMovement.has_value())
        {
            return followPlatformerPath(
                actor.body,
                *actor.platformerMovement,
                follower,
                update.deltaTime,
                actor.surfaceClimb.has_value() ? &*actor.surfaceClimb : nullptr);
        }
        return {};
    }
}
