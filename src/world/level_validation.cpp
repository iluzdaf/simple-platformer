#include "simple_platformer/world/level_validation.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace
{
    constexpr float InsideBody = 0.001F;

    int tileContaining(float position)
    {
        return static_cast<int>(
            std::floor(position / static_cast<float>(simple_platformer::TileSize)));
    }

    bool hasClearance(const simple_platformer::TileMap& map, const simple_platformer::Aabb& bounds)
    {
        const int firstColumn = tileContaining(bounds.position.x + InsideBody);
        const int lastColumn = tileContaining(bounds.position.x + bounds.size.x - InsideBody);
        const int firstRow = tileContaining(bounds.position.y + InsideBody);
        const int lastRow = tileContaining(bounds.position.y + bounds.size.y - InsideBody);

        for (int row = firstRow; row <= lastRow; ++row)
        {
            for (int column = firstColumn; column <= lastColumn; ++column)
            {
                if (map.blocksMovement({column, row}))
                {
                    return false;
                }
            }
        }
        return true;
    }

    bool hasGroundSupport(
        const simple_platformer::TileMap& map,
        const simple_platformer::Aabb& bounds)
    {
        const int firstColumn = tileContaining(bounds.position.x + InsideBody);
        const int lastColumn = tileContaining(bounds.position.x + bounds.size.x - InsideBody);
        const int rowBelow = tileContaining(bounds.position.y + bounds.size.y + InsideBody);

        for (int column = firstColumn; column <= lastColumn; ++column)
        {
            if (map.blocksMovement({column, rowBelow}))
            {
                return true;
            }
        }
        return false;
    }

    std::string actorLocation(int level, simple_platformer::ActorId actor, std::string_view place)
    {
        return "Level " + std::to_string(level) + " actor " + std::to_string(actor.value) + " " +
               std::string(place);
    }

    void validatePlacement(
        const simple_platformer::TileMap& map,
        const simple_platformer::Actor& actor,
        const simple_platformer::Aabb& bounds,
        int level,
        std::string_view place)
    {
        const std::string location = actorLocation(level, actor.id, place);
        if (!hasClearance(map, bounds))
        {
            throw std::invalid_argument(location + " overlaps a blocked tile");
        }
        if (actor.platformerMovement.has_value() && !hasGroundSupport(map, bounds))
        {
            throw std::invalid_argument(location + " has no ground support");
        }
    }

    void validateAtFeet(
        const simple_platformer::TileMap& map,
        const simple_platformer::Actor& actor,
        glm::vec2 feet,
        int level,
        std::string_view place)
    {
        simple_platformer::Aabb bounds{{0.0F, 0.0F}, actor.body.bounds.size};
        simple_platformer::placeFeetAt(bounds, feet);
        validatePlacement(map, actor, bounds, level, place);
    }
}

namespace simple_platformer
{
    void validateLevelActors(const TileMap& map, const World& world, int level)
    {
        for (const Actor& actor : world.actors())
        {
            validatePlacement(map, actor, actor.body.bounds, level, "spawn");
            if (!actor.patrol.has_value())
            {
                continue;
            }
            validateAtFeet(map, actor, actor.patrol->firstFeet, level, "first patrol point");
            validateAtFeet(map, actor, actor.patrol->secondFeet, level, "second patrol point");
        }
    }
}
