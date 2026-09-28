#include "simple_platformer/navigation/flying_navigation.hpp"

#include <array>
#include <utility>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_search.hpp"
#include "simple_platformer/timing/frame_profile.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    std::vector<GridPosition> flyingNeighbors(const TileMap& map, GridPosition cell)
    {
        constexpr std::array<GridPosition, 4> Directions{
            GridPosition{-1, 0}, GridPosition{1, 0}, GridPosition{0, -1}, GridPosition{0, 1}};

        std::vector<GridPosition> neighbors;
        for (const GridPosition direction : Directions)
        {
            const GridPosition candidate{cell.x + direction.x, cell.y + direction.y};
            if (map.contains(candidate) && !map.blocksMovement(candidate))
            {
                neighbors.push_back(candidate);
            }
        }
        return neighbors;
    }

    std::vector<NavigationConnection> flyingConnections(const TileMap& map, GridPosition cell)
    {
        std::vector<NavigationConnection> connections;
        for (const GridPosition neighbor : flyingNeighbors(map, cell))
        {
            connections.push_back({{neighbor, Traversal::Fly, {}}, 1});
        }
        return connections;
    }

    NavigationPathResult findFlyingPath(
        const TileMap& map,
        GridPosition start,
        GridPosition goal,
        FrameProfile* profile)
    {
        addFrameStatistic(profile, "Navigation", "Path searches");
        if (!map.contains(start) || !map.contains(goal))
        {
            return {NavigationPathStatus::Unreachable, {}};
        }
        int cellsExpanded = 0;
        const GridConnectionFunction connections =
            [&map, profile, &cellsExpanded](GridPosition cell)
        {
            const PhaseScope connectionPhase(profile, "Navigation", "Connection retrieval");
            ++cellsExpanded;
            return flyingConnections(map, cell);
        };
        const GridNeighborFunction neighbors = [&map, profile](GridPosition cell)
        {
            const PhaseScope neighborPhase(profile, "Navigation", "Neighbor generation");
            return flyingNeighbors(map, cell);
        };

        PathSearchResult result;
        {
            const PhaseScope algorithmPhase(profile, "Navigation", "Search algorithm");
            result = findLowestCostPath(
                start, goal, map.size(), neighbors, connections, manhattanHeuristic);
        }
        addFrameStatistic(profile, "Navigation", "Cells expanded", cellsExpanded);
        if (result.status == PathSearchStatus::Found)
        {
            return {NavigationPathStatus::Found, std::move(result.path)};
        }
        return {NavigationPathStatus::Unreachable, {}};
    }
}
