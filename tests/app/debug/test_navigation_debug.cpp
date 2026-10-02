#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "debug/navigation_debug.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/navigation/actor_navigation.hpp"
#include "simple_platformer/navigation/platformer_connection_table.hpp"
#include "simple_platformer/navigation/traversal.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/fixed_step.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    // What a level start does: build the connections for each NPC's profile.
    void prepare(const simple_platformer::TileMap& map, simple_platformer::World& world)
    {
        simple_platformer::prepareNavigation(map, world, tests::FixedStepSeconds);
    }

    // What the next world step does after a tile breaks.
    void applyBreaks(const simple_platformer::TileMap& map, simple_platformer::World& world)
    {
        world.platformerConnections().applyRecordedTileBreaks(map);
    }
}

TEST_CASE(
    "Navigation debug data shows the connection table per cell",
    "[app][debug][navigation]")
{
    simple_platformer::TileMap map =
        tests::TileMapBuilder({".....", ".....", "##g##"})
            .where('g', tests::Tile().blocksMovement().breaksInto('.'));
    simple_platformer::World world;
    // Without a platformer NPC there is nothing to show.
    REQUIRE_FALSE(
        simple_platformer::makeNavigationConnectionsDebugInfo(world, map, tests::FixedStepSeconds)
            .has_value());

    world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                       .atFeet({8.0F, 32.0F})
                       .platforming()
                       .thinking({64.0F, 1.0F}));
    const auto cellsOf = [&]
    {
        return simple_platformer::makeNavigationConnectionsDebugInfo(
                   world, map, tests::FixedStepSeconds)
            .value_or(simple_platformer::NavigationConnectionsDebugInfo{})
            .cells;
    };
    // Before the level prepares navigation, the standable cells are listed without counts.
    std::vector<simple_platformer::NavigationCellDebugInfo> cells = cellsOf();
    REQUIRE(cells.size() == 5);
    REQUIRE(cells.front().bounds.topLeft == glm::vec2{0.0F, 16.0F});
    REQUIRE(cells.front().bounds.size == glm::vec2{16.0F, 16.0F});
    REQUIRE(std::none_of(
        cells.begin(),
        cells.end(),
        [](const simple_platformer::NavigationCellDebugInfo& cell)
        { return cell.connections.has_value(); }));

    prepare(map, world);
    cells = cellsOf();
    REQUIRE(std::all_of(
        cells.begin(),
        cells.end(),
        [](const simple_platformer::NavigationCellDebugInfo& cell)
        { return cell.connections.has_value() && *cell.connections > 0; }));

    // Once the table applies a break, the cells it rebuilt are shown again.
    REQUIRE(map.breakTile({2, 2}));
    applyBreaks(map, world);
    cells = cellsOf();
    // The cell over the hole can no longer be stood on, so it is not listed; the hole
    // itself can, since the map's floor blocks beneath it.
    REQUIRE(cells.size() == 5);
    const auto listed = [&cells](glm::vec2 position)
    {
        return std::any_of(
            cells.begin(),
            cells.end(),
            [position](const simple_platformer::NavigationCellDebugInfo& cell)
            { return cell.bounds.topLeft == position; });
    };
    REQUIRE_FALSE(listed({32.0F, 16.0F}));
    REQUIRE(listed({32.0F, 32.0F}));
    REQUIRE(std::none_of(
        cells.begin(),
        cells.end(),
        [](const simple_platformer::NavigationCellDebugInfo& cell)
        { return !cell.connections.has_value(); }));
}

TEST_CASE(
    "Navigation debug data shows the selected traversal profile and connection totals",
    "[app][debug][navigation]")
{
    simple_platformer::TileMap map =
        tests::TileMapBuilder({".....", ".....", "##g##"})
            .where('g', tests::Tile().blocksMovement().breaksInto('.'));
    simple_platformer::World world;
    world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                       .atFeet({8.0F, 32.0F})
                       .platforming()
                       .thinking({64.0F, 1.0F}));
    world.addActor(tests::ActorBuilder::sized({12.0F, 20.0F})
                       .atFeet({40.0F, 32.0F})
                       .platforming()
                       .thinking({64.0F, 1.0F}));
    prepare(map, world);
    const auto infoFor = [&](std::size_t profileIndex)
    {
        simple_platformer::NavigationDebugView view;
        view.profileIndex = profileIndex;
        view.namedProfiles = {
            {"soldier", {{12.0F, 20.0F}, {}, tests::FixedStepSeconds}},
            {"zombie", {{12.0F, 12.0F}, {}, tests::FixedStepSeconds}}};
        return simple_platformer::makeNavigationConnectionsDebugInfo(
                   world, map, tests::FixedStepSeconds, view)
            .value_or(simple_platformer::NavigationConnectionsDebugInfo{});
    };

    // The index picks a profile in the order first found and wraps; an actor name is
    // shown when the matching profile has one.
    REQUIRE(
        simple_platformer::makeNavigationConnectionsDebugInfo(world, map, tests::FixedStepSeconds)
            .value_or(simple_platformer::NavigationConnectionsDebugInfo{})
            .actorName.empty());
    REQUIRE(infoFor(0).profileCount == 2);
    REQUIRE(infoFor(0).profileIndex == 0);
    REQUIRE(infoFor(0).bodySize == glm::vec2{12.0F, 12.0F});
    REQUIRE(infoFor(0).actorName == "zombie");
    REQUIRE(infoFor(1).profileIndex == 1);
    REQUIRE(infoFor(1).bodySize == glm::vec2{12.0F, 20.0F});
    REQUIRE(infoFor(1).actorName == "soldier");
    REQUIRE(infoFor(2).profileIndex == 0);

    REQUIRE(infoFor(0).cellsConnected == 5);
    REQUIRE(map.breakTile({2, 2}));
    applyBreaks(map, world);
    REQUIRE(infoFor(0).cells.size() == 5);
    REQUIRE(infoFor(0).cellsConnected > 0);
}

TEST_CASE(
    "Navigation debug data shows the cursor cell's connections and footprint",
    "[app][debug][navigation]")
{
    // A floor with a step up at the right, so the cursor cell has walks and jumps.
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "......##", "########"});
    simple_platformer::World world;
    world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                       .atFeet({8.0F, 32.0F})
                       .platforming()
                       .thinking({64.0F, 1.0F}));
    const auto infoAt = [&](std::optional<glm::vec2> cursor)
    {
        simple_platformer::NavigationDebugView view;
        view.cursorWorld = cursor;
        return simple_platformer::makeNavigationConnectionsDebugInfo(
                   world, map, tests::FixedStepSeconds, view)
            .value_or(simple_platformer::NavigationConnectionsDebugInfo{});
    };

    // No cursor, or one off the map, shows no cell; before the table is built, a cell
    // shows its bounds only.
    REQUIRE_FALSE(infoAt(std::nullopt).cursorCell.has_value());
    REQUIRE_FALSE(infoAt(glm::vec2{-1.0F, 20.0F}).cursorCell.has_value());
    const simple_platformer::CursorCellDebugInfo unbuilt =
        infoAt(glm::vec2{40.0F, 20.0F})
            .cursorCell.value_or(simple_platformer::CursorCellDebugInfo{});
    REQUIRE(unbuilt.bounds.topLeft == glm::vec2{32.0F, 16.0F});
    REQUIRE_FALSE(unbuilt.footprint.has_value());
    REQUIRE(unbuilt.connections.empty());

    prepare(map, world);

    const simple_platformer::CursorCellDebugInfo built =
        infoAt(glm::vec2{40.0F, 20.0F})
            .cursorCell.value_or(simple_platformer::CursorCellDebugInfo{});
    REQUIRE(built.footprint.has_value());
    const simple_platformer::Aabb footprint = built.footprint.value_or(simple_platformer::Aabb{});
    REQUIRE(footprint.topLeft.x <= 32.0F);
    REQUIRE(simple_platformer::rightOf(footprint) >= 48.0F);
    REQUIRE_FALSE(built.connections.empty());
    bool sawWalk = false;
    bool sawArc = false;
    for (const simple_platformer::ConnectionDebugInfo& connection : built.connections)
    {
        REQUIRE(connection.fromFeet == glm::vec2{40.0F, 32.0F});
        REQUIRE(connection.cost > 0);
        if (connection.traversal == simple_platformer::Traversal::Walk)
        {
            sawWalk = true;
            REQUIRE(connection.sampledFeet.empty());
        }
        else
        {
            sawArc = true;
            REQUIRE(connection.sampledFeet.size() > 1);
            REQUIRE(connection.sampledFeet.front() == connection.fromFeet);
        }
    }
    REQUIRE(sawWalk);
    REQUIRE(sawArc);
}

TEST_CASE(
    "Navigation debug data lists the cells a climber can only hold a wall or ceiling in",
    "[app][debug][navigation][climb]")
{
    // A room walled and roofed with climbable tiles.
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"cccccc", "c.....", "c.....", "c.....", "######"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    const auto infoFor = [&map](simple_platformer::World& world)
    {
        prepare(map, world);
        return simple_platformer::makeNavigationConnectionsDebugInfo(
                   world, map, tests::FixedStepSeconds)
            .value_or(simple_platformer::NavigationConnectionsDebugInfo{});
    };
    const auto isListed =
        [](const simple_platformer::NavigationConnectionsDebugInfo& info, glm::vec2 position)
    {
        return std::any_of(
            info.cells.begin(),
            info.cells.end(),
            [position](const simple_platformer::NavigationCellDebugInfo& cell)
            { return cell.bounds.topLeft == position; });
    };

    // A walker is shown only the floor.
    simple_platformer::World walkers;
    walkers.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                         .inCell({3, 3})
                         .platforming()
                         .thinking({64.0F, 1.0F}));
    const simple_platformer::NavigationConnectionsDebugInfo walking = infoFor(walkers);
    REQUIRE(walking.cells.size() == 5);
    REQUIRE_FALSE(isListed(walking, {16.0F, 16.0F}));

    // A climber is also shown the cells along the wall and under the ceiling, marked as
    // ones it cannot stand in, and they are counted.
    simple_platformer::World climbers;
    climbers.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                          .inCell({3, 3})
                          .platforming()
                          .climbing({60.0F})
                          .thinking({64.0F, 1.0F}));

    const simple_platformer::NavigationConnectionsDebugInfo climbing = infoFor(climbers);
    REQUIRE(climbing.cells.size() > walking.cells.size());
    for (const simple_platformer::NavigationCellDebugInfo& cell : climbing.cells)
    {
        // The floor row stands; the rows above are held on the wall or ceiling.
        REQUIRE(cell.standable == (cell.bounds.topLeft.y == 48.0F));
        REQUIRE(cell.connections.has_value());
        REQUIRE(cell.connections.value_or(0) > 0);
    }
    REQUIRE(isListed(climbing, {16.0F, 16.0F}));
    REQUIRE(isListed(climbing, {16.0F, 32.0F}));
    REQUIRE(isListed(climbing, {48.0F, 16.0F}));
}
