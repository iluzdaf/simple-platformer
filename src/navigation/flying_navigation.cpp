#include "simple_platformer/navigation/flying_navigation.hpp"

#include <array>
#include <optional>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_search.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    std::optional<NavigationPath> findFlyingPath(
        const TileMap& map,
        GridPosition start,
        GridPosition goal,
        PathSearchStatistics* statistics)
    {
        if (!map.contains(start) || !map.contains(goal))
        {
            return std::nullopt;
        }
        const GridNeighborFunction neighbors =
            [&map](GridPosition cell, const GridNeighborVisitor& visit)
        {
            for (const NavigationNeighbor& neighbor : flyingNeighbors(map, cell))
            {
                visit(neighbor, neighbor.cost);
            }
        };

        // Call the overload without a heuristic to compare A* with a plain lowest-cost search.
        return findLowestCostPath(
            start, goal, map.size(), neighbors, manhattanHeuristic, statistics);
    }

    std::vector<NavigationNeighbor> flyingNeighbors(const TileMap& map, GridPosition cell)
    {
        constexpr std::array<GridPosition, 4> Directions{
            GridPosition{-1, 0}, GridPosition{1, 0}, GridPosition{0, -1}, GridPosition{0, 1}};

        std::vector<NavigationNeighbor> neighbors;
        for (const GridPosition direction : Directions)
        {
            const GridPosition candidate{cell.x + direction.x, cell.y + direction.y};
            if (map.contains(candidate) && !map.blocksMovement(candidate))
            {
                neighbors.push_back({candidate, Traversal::Fly, 1, {}});
            }
        }
        return neighbors;
    }
}
