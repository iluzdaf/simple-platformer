#pragma once

#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/platformer_connection_cache.hpp"
#include "simple_platformer/navigation/navigation_fill.hpp"
#include "simple_platformer/navigation/route.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/tile_size.hpp"

namespace tests
{
    // The waypoints a route of floor steps from the start cell gives, for tests that
    // assemble a path from simulated connections.
    inline simple_platformer::NavigationPath floorPath(
        simple_platformer::Cell start,
        std::vector<simple_platformer::RouteStep> steps)
    {
        using simple_platformer::feetInCell;

        simple_platformer::NavigationPath path{feetInCell(TileSize, start), {}};
        for (simple_platformer::RouteStep& step : steps)
        {
            path.waypoints.push_back(
                {feetInCell(TileSize, step.destination.cell),
                 step.traversal,
                 std::move(step.inputs)});
        }
        return path;
    }

    // Queues every cell of the map for the profile and fills them all, as the game
    // does for each NPC profile over the first steps of a level.
    inline void fillConnections(
        const simple_platformer::TileMap& map,
        simple_platformer::PlatformerConnectionCache& cache,
        const simple_platformer::PlatformerTraversalProfile& profile)
    {
        for (int row = 0; row < map.height(); ++row)
        {
            for (int column = 0; column < map.width(); ++column)
            {
                cache.queue({column, row}, profile);
            }
        }
        while (simple_platformer::advanceNavigationFill(
                   map, cache, simple_platformer::NavigationFillTicksPerStep) > 0)
        {
        }
    }
}
