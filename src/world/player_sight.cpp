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
    bool playerCanSee(const TileMap& map, const World& world, const Aabb& bounds)
    {
        const glm::vec2 center = centerOf(bounds);
        if (!map.blocksSight(worldToGrid(center)))
        {
            return true;
        }
        const Actor* player = world.findActor(world.playerId());
        return player != nullptr &&
               !segmentCastSightBlockingTiles(map, centerOf(player->body.bounds), center)
                    .has_value();
    }
}
