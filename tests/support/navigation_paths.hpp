#pragma once

#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/route.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
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

}
