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
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    namespace
    {
        // In pixels: keep the path until the goal moves farther than this, so a drifting
        // target does not trigger a search every update.
        constexpr float ReplanDistance = 8.0F;

        // Needs a path if none is stored, the planned goal has moved, or the actor has
        // been displaced from the end of a finished path.
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

        // Keeps a usable path or searches again after a goal change, displacement, or
        // tile break. New connections use the same step as the actor's next movement.
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

            std::optional<NavigationPathResult> pathResult = findActorPath(
                map, actor, goalFeet, update.deltaTime, update.world.platformerConnections());
            if (!pathResult.has_value())
            {
                return;
            }
            if (!pathResult->path.has_value())
            {
                clearPath(follower);
            }
            follower.goal = goalFeet;
            follower.breaksWhenPlanned = map.brokenCells().size();
            if (pathResult->path.has_value())
            {
                setPath(follower, std::move(pathResult->path.value()));
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
