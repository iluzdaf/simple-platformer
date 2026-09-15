#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/flying_navigation.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/world/tile_map.hpp"

TEST_CASE("Flying neighbors stay inside the map and avoid solid cells", "[navigation][flying]")
{
    const simple_platformer::TileMap map =
        simple_platformer::TileMap::fromAscii({"...", ".#.", "###"});

    const std::vector<simple_platformer::NavigationNeighbor> neighbors =
        simple_platformer::flyingNeighbors(map, {0, 0});

    REQUIRE(neighbors.size() == 2);
    const auto hasDestination = [&neighbors](simple_platformer::GridPosition destination)
    {
        return std::find_if(
                   neighbors.begin(),
                   neighbors.end(),
                   [destination](const simple_platformer::NavigationNeighbor& neighbor)
                   { return neighbor.destination == destination; }) != neighbors.end();
    };
    REQUIRE(hasDestination({1, 0}));
    REQUIRE(hasDestination({0, 1}));
    REQUIRE(neighbors.front().traversal == simple_platformer::Traversal::Fly);
    REQUIRE(simple_platformer::flyingNeighbors(map, {1, 0}).size() == 2);
}
