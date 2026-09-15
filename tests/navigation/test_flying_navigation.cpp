#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/flying_navigation.hpp"
#include "simple_platformer/world/tile_map.hpp"

TEST_CASE("Flying neighbors stay inside the map and avoid solid cells", "[navigation][flying]")
{
    const simple_platformer::TileMap map =
        simple_platformer::TileMap::fromAscii({"...", ".#.", "###"});

    const std::vector<simple_platformer::GridPosition> neighbors =
        simple_platformer::flyingNeighbors(map, {0, 0});

    REQUIRE(neighbors.size() == 2);
    REQUIRE(
        std::find(neighbors.begin(), neighbors.end(), simple_platformer::GridPosition{1, 0}) !=
        neighbors.end());
    REQUIRE(
        std::find(neighbors.begin(), neighbors.end(), simple_platformer::GridPosition{0, 1}) !=
        neighbors.end());
    REQUIRE(simple_platformer::flyingNeighbors(map, {1, 0}).size() == 2);
}
