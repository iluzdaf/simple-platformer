#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/flying_navigation.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_search.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/tile_map_builder.hpp"

TEST_CASE("Flying neighbors stay inside the map and avoid solid cells", "[navigation][flying]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"...", ".#.", "###"});

    const std::vector<simple_platformer::NavigationNeighbor> neighbors =
        simple_platformer::flyingNeighbors(map, {0, 0});

    REQUIRE(neighbors.size() == 2);
    const auto hasDestination = [&neighbors](simple_platformer::GridPosition destinationCell)
    {
        return std::find_if(
                   neighbors.begin(),
                   neighbors.end(),
                   [destinationCell](const simple_platformer::NavigationNeighbor& neighbor)
                   { return neighbor.destinationCell == destinationCell; }) != neighbors.end();
    };
    REQUIRE(hasDestination({1, 0}));
    REQUIRE(hasDestination({0, 1}));
    REQUIRE(neighbors.front().traversal == simple_platformer::Traversal::Fly);
    REQUIRE(simple_platformer::flyingNeighbors(map, {1, 0}).size() == 2);
}

TEST_CASE("Flying path search uses the flying navigation policy", "[navigation][flying]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", ".##.", "...."});

    simple_platformer::PathSearchStatistics cost;
    const auto path = simple_platformer::findFlyingPath(map, {0, 1}, {3, 1}, &cost);

    REQUIRE(path.has_value());
    const simple_platformer::NavigationPath route =
        path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(route.start == simple_platformer::GridPosition{0, 1});
    REQUIRE(route.steps.back().destinationCell == simple_platformer::GridPosition{3, 1});
    // A flying search expands cells but simulates no movement.
    REQUIRE(cost.nodesExpanded >= 1);
    REQUIRE(cost.simulatedTicks == 0);
}
