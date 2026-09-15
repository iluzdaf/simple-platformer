#include "simple_platformer/navigation/flying_navigation.hpp"

#include <array>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    std::vector<GridPosition> flyingNeighbors(const TileMap& map, GridPosition position)
    {
        constexpr std::array<GridPosition, 4> Directions{
            GridPosition{-1, 0}, GridPosition{1, 0}, GridPosition{0, -1}, GridPosition{0, 1}};

        std::vector<GridPosition> neighbors;
        for (const GridPosition direction : Directions)
        {
            const GridPosition candidate{position.x + direction.x, position.y + direction.y};
            if (map.contains(candidate) && !map.isSolid(candidate))
            {
                neighbors.push_back(candidate);
            }
        }
        return neighbors;
    }
}
