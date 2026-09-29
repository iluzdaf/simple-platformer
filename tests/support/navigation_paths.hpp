#pragma once

#include <utility>
#include <vector>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/connection_cache.hpp"
#include "simple_platformer/navigation/navigation_fill.hpp"
#include "simple_platformer/navigation/navigation_graph.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/platformer_cells.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/actor_builder.hpp"
#include "support/tile_size.hpp"

namespace tests
{
    // The profile's body resting at the location, where a search can start from it.
    inline simple_platformer::Aabb restingBody(
        simple_platformer::NavigationLocation location,
        const simple_platformer::PlatformerTraversalProfile& profile)
    {
        return simple_platformer::boundsAtSurface(TileSize, location, profile.size);
    }

    // The walker a profile describes, with this body; a climber when the profile can
    // climb. Navigation reads the profile back from it.
    inline simple_platformer::Actor actorFor(
        const simple_platformer::Aabb& body,
        const simple_platformer::PlatformerTraversalProfile& profile)
    {
        ActorBuilder walker =
            ActorBuilder::sized(body.size).at(body.position).walking(profile.movement);
        if (profile.climb.has_value())
        {
            return std::move(walker).climbing(*profile.climb);
        }
        return std::move(walker);
    }

    // The waypoints a route of floor steps from the start cell gives, for tests that
    // assemble a path from simulated connections.
    inline simple_platformer::NavigationPath floorPath(
        simple_platformer::Cell start,
        std::vector<simple_platformer::NavigationStep> steps)
    {
        simple_platformer::NavigationPath path{simple_platformer::feetInCell(TileSize, start), {}};
        for (simple_platformer::NavigationStep& step : steps)
        {
            path.waypoints.push_back(
                {simple_platformer::feetInCell(TileSize, step.destinationCell),
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
