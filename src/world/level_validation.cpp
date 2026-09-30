#include "simple_platformer/world/level_validation.hpp"

#include <stdexcept>
#include <string>
#include <string_view>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    namespace
    {
        bool hasClearance(const TileMap& map, const Aabb& bounds)
        {
            const CellRange cells = cellsCovered(map.tileSize(), bounds);
            for (int row = cells.first.y; row <= cells.last.y; ++row)
            {
                for (int column = cells.first.x; column <= cells.last.x; ++column)
                {
                    if (map.blocksMovement({column, row}))
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        bool hasGroundSupport(const TileMap& map, const Aabb& bounds)
        {
            const CellRange cells = cellsCovered(map.tileSize(), bounds);
            const int rowBelow =
                cellAt(map.tileSize(), {bounds.topLeft.x, bottomOf(bounds) + EdgeTolerance}).y;

            for (int column = cells.first.x; column <= cells.last.x; ++column)
            {
                if (map.blocksMovement({column, rowBelow}))
                {
                    return true;
                }
            }
            return false;
        }

        std::string actorLocation(int level, ActorId actor, std::string_view place)
        {
            return "Level " + std::to_string(level) + " actor " + std::to_string(actor.value) +
                   " " + std::string(place);
        }

        void validatePlacement(
            const TileMap& map,
            const Actor& actor,
            const Aabb& bounds,
            int level,
            std::string_view place,
            bool needsGround)
        {
            const std::string location = actorLocation(level, actor.id, place);
            if (!hasClearance(map, bounds))
            {
                throw std::invalid_argument(location + " overlaps a blocked tile");
            }
            if (needsGround && !hasGroundSupport(map, bounds))
            {
                throw std::invalid_argument(location + " has no ground support");
            }
        }

        void validateAtFeet(
            const TileMap& map,
            const Actor& actor,
            glm::vec2 feet,
            int level,
            std::string_view place,
            bool needsGround)
        {
            const Aabb bounds = boxStandingOn(feet, actor.body.bounds.size);
            validatePlacement(map, actor, bounds, level, place, needsGround);
        }

        // A climber can patrol to a wall or ceiling; navigation takes it to the nearest
        // place it can hold.
        bool patrolNeedsGround(const Actor& actor)
        {
            return actor.platformerMovement.has_value() && !actor.surfaceClimb.has_value();
        }
    }

    void validateLevelActors(const TileMap& map, const World& world, int level)
    {
        for (const Actor& actor : world.actors())
        {
            const bool platformer = actor.platformerMovement.has_value();
            validatePlacement(map, actor, actor.body.bounds, level, "spawn", platformer);
            if (!actor.patrol.has_value())
            {
                continue;
            }
            const bool needsGround = patrolNeedsGround(actor);
            validateAtFeet(
                map, actor, actor.patrol->firstFeet, level, "first patrol point", needsGround);
            validateAtFeet(
                map, actor, actor.patrol->secondFeet, level, "second patrol point", needsGround);
        }

        const Actor* player = world.findActor(world.playerId());
        if (player != nullptr)
        {
            validateAtFeet(
                map,
                *player,
                world.playerSpawnFeet(),
                level,
                "respawn",
                player->platformerMovement.has_value());
        }
    }
}
