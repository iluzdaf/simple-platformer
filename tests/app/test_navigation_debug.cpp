#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

#include <glm/vec2.hpp>

#include "debug/navigation_debug.hpp"
#include "simple_platformer/navigation/connection_cache.hpp"
#include "simple_platformer/npc/npc_system.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/fixed_step.hpp"
#include "support/tile_map_builder.hpp"

TEST_CASE(
    "Navigation debug data shows what the connection cache keeps per cell",
    "[app][debug][navigation]")
{
    simple_platformer::TileMap map =
        tests::TileMapBuilder({".....", ".....", "##g##"})
            .where('g', tests::Tile().blocksMovement().breaksInto('.'));
    simple_platformer::World world;
    // Without a platformer NPC there is nothing to show.
    REQUIRE_FALSE(
        simple_platformer::makeNavigationCacheDebugInfo(world, map, tests::FixedStepSeconds)
            .has_value());

    world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                       .atFeet({8.0F, 32.0F})
                       .walking()
                       .thinking({64.0F, 1.0F}));
    const auto cellsOf = [&]
    {
        return simple_platformer::makeNavigationCacheDebugInfo(world, map, tests::FixedStepSeconds)
            .value_or(simple_platformer::NavigationCacheDebugInfo{})
            .cells;
    };
    // Every standable cell is listed; before anything is kept, none has connections.
    std::vector<simple_platformer::NavigationCellDebugInfo> cells = cellsOf();
    REQUIRE(cells.size() == 5);
    REQUIRE(cells.front().bounds.position == glm::vec2{0.0F, 16.0F});
    REQUIRE(cells.front().bounds.size == glm::vec2{16.0F, 16.0F});
    REQUIRE(std::all_of(
        cells.begin(),
        cells.end(),
        [](const simple_platformer::NavigationCellDebugInfo& cell)
        { return !cell.connections.has_value(); }));

    // Warmed, every cell has its connections counted.
    simple_platformer::warmNpcNavigation(map, world, tests::FixedStepSeconds);
    cells = cellsOf();
    REQUIRE(std::all_of(
        cells.begin(),
        cells.end(),
        [](const simple_platformer::NavigationCellDebugInfo& cell)
        { return cell.connections.has_value() && *cell.connections > 0; }));

    // A break the cache has synced with leaves the cells it dropped missing.
    REQUIRE(map.breakTile({2, 2}));
    world.platformerConnections().syncWith(map);
    cells = cellsOf();
    // The cell over the hole can no longer be stood on, so it is not listed; the hole
    // itself can, since the map's floor blocks beneath it, and it was never kept.
    REQUIRE(cells.size() == 5);
    const auto listed = [&cells](glm::vec2 position)
    {
        return std::any_of(
            cells.begin(),
            cells.end(),
            [position](const simple_platformer::NavigationCellDebugInfo& cell)
            { return cell.bounds.position == position; });
    };
    REQUIRE_FALSE(listed({32.0F, 16.0F}));
    REQUIRE(listed({32.0F, 32.0F}));
    REQUIRE(std::none_of(
        cells.begin(),
        cells.end(),
        [](const simple_platformer::NavigationCellDebugInfo& cell)
        { return cell.connections.has_value(); }));
}
