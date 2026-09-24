#include "navigation_debug.hpp"

#include <cstddef>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/connection_cache.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    std::optional<NavigationCacheDebugInfo> makeNavigationCacheDebugInfo(
        const World& world,
        const TileMap& map,
        float simulationStepSeconds)
    {
        std::optional<ConnectionBody> found;
        for (const Actor& actor : world.actors())
        {
            if (actor.pathFollower.has_value() && actor.platformerMovement.has_value())
            {
                found = ConnectionBody{
                    actor.body.bounds.size,
                    actor.platformerMovement.value().config,
                    simulationStepSeconds};
                break;
            }
        }
        if (!found.has_value())
        {
            return std::nullopt;
        }
        const ConnectionBody body = found.value_or(ConnectionBody{});
        const PlatformerConnectionCache& cache = world.platformerConnections();
        const auto tileSize = static_cast<float>(map.tileSize());
        NavigationCacheDebugInfo info;
        info.bodySize = body.size;
        for (int row = 0; row < map.height(); ++row)
        {
            for (int column = 0; column < map.width(); ++column)
            {
                const GridPosition cell{column, row};
                if (!canStandAt(map, cell, body.size))
                {
                    continue;
                }
                const std::vector<NavigationNeighbor>* kept = cache.find(cell, body);
                info.cells.push_back(
                    {{{static_cast<float>(column) * tileSize, static_cast<float>(row) * tileSize},
                      {tileSize, tileSize}},
                     kept == nullptr ? std::nullopt : std::optional<std::size_t>(kept->size())});
            }
        }
        return info;
    }
}
