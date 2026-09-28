#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/flying_navigation.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/timing/frame_profile.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/tile_map_builder.hpp"

TEST_CASE("Flying connections lead to adjacent open cells at cost one", "[navigation][flying]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"...", ".#.", "###"});
    const std::vector<simple_platformer::NavigationConnection> connections =
        simple_platformer::flyingConnections(map, {0, 0});

    REQUIRE(connections.size() == 2);
    REQUIRE(connections[0].step.destinationCell == simple_platformer::GridPosition{1, 0});
    REQUIRE(connections[1].step.destinationCell == simple_platformer::GridPosition{0, 1});
    for (const auto& connection : connections)
    {
        REQUIRE(connection.step.traversal == simple_platformer::Traversal::Fly);
        REQUIRE(connection.step.inputs.empty());
        REQUIRE(connection.cost == 1);
    }
    REQUIRE(simple_platformer::flyingConnections(map, {1, 0}).size() == 2);
}

TEST_CASE("A flying path crosses open cells around a wall", "[navigation][flying]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", ".##.", "...."});

    simple_platformer::FrameProfile profile;
    const simple_platformer::NavigationPathResult result =
        simple_platformer::findFlyingPath(map, {0, 1}, {3, 1}, &profile);

    REQUIRE(result.status == simple_platformer::NavigationPathStatus::Found);
    REQUIRE(result.path.has_value());
    const simple_platformer::NavigationPath route =
        result.path.value_or(simple_platformer::NavigationPath{});
    REQUIRE(route.start == simple_platformer::GridPosition{0, 1});
    REQUIRE(route.steps.back().destinationCell == simple_platformer::GridPosition{3, 1});
    REQUIRE(route.steps.front().traversal == simple_platformer::Traversal::Fly);
    // A flying search expands cells but simulates no movement.
    REQUIRE(simple_platformer::frameStatisticCount(profile, "Cells expanded") >= 1);
    REQUIRE(simple_platformer::frameStatisticCount(profile, "Search simulated ticks") == 0);
}

TEST_CASE("Flying paths report unreachable destinations", "[navigation][flying]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({".#."});

    const simple_platformer::NavigationPathResult blocked =
        simple_platformer::findFlyingPath(map, {0, 0}, {2, 0});
    REQUIRE(blocked.status == simple_platformer::NavigationPathStatus::Unreachable);
    REQUIRE_FALSE(blocked.path.has_value());

    const simple_platformer::NavigationPathResult offMap =
        simple_platformer::findFlyingPath(map, {0, 0}, {3, 0});
    REQUIRE(offMap.status == simple_platformer::NavigationPathStatus::Unreachable);
    REQUIRE_FALSE(offMap.path.has_value());
}
