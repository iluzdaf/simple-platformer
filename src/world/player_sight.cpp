#include "simple_platformer/world/player_sight.hpp"

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/physics/segment_cast.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    namespace
    {
        bool standsInCover(const TileMap& map, const Aabb& bounds)
        {
            return map.blocksSight(worldToGrid(centerOf(bounds)));
        }
    }

    bool playerCanSee(const TileMap& map, const World& world, const Aabb& bounds)
    {
        if (!standsInCover(map, bounds))
        {
            return true;
        }
        const Actor* player = world.findActor(world.playerId());
        return player != nullptr &&
               lineOfSight(map, centerOf(player->body.bounds), centerOf(bounds));
    }
}
