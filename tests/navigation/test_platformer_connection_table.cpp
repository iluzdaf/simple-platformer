#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/navigation/actor_navigation.hpp"
#include "simple_platformer/navigation/platformer_connection_table.hpp"
#include "simple_platformer/navigation/platformer_connections.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/navigation/route.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/fixed_step.hpp"
#include "support/route_connections.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    using simple_platformer::Cell;
    using simple_platformer::PlatformerConnectionTable;
    using simple_platformer::PlatformerTraversalProfile;
    using simple_platformer::TileMap;

    const PlatformerTraversalProfile Walker{{12.0F, 12.0F}, {}, tests::FixedStepSeconds};
    const PlatformerTraversalProfile Climber{
        {12.0F, 12.0F},
        {},
        tests::FixedStepSeconds,
        simple_platformer::SurfaceClimbConfig{60.0F}};

    // Every cell of the table holds what simulating that cell on the map gives now.
    void requireMatchesTheMap(
        const PlatformerConnectionTable& table,
        const TileMap& map,
        const PlatformerTraversalProfile& profile)
    {
        for (int row = 0; row < map.height(); ++row)
        {
            for (int column = 0; column < map.width(); ++column)
            {
                const Cell cell{column, row};
                CAPTURE(column, row);
                const auto fresh =
                    simple_platformer::buildPlatformerConnections(map, cell, profile);
                tests::requireSameRouteConnections(
                    table.connections(cell, profile), fresh.connections);
                REQUIRE(table.footprint(cell, profile).first == fresh.footprint.first);
                REQUIRE(table.footprint(cell, profile).last == fresh.footprint.last);
            }
        }
    }

    // How many of the table's cells have a footprint holding the cell.
    int cellsWhoseFootprintHolds(
        const PlatformerConnectionTable& table,
        const TileMap& map,
        const PlatformerTraversalProfile& profile,
        Cell held)
    {
        int count = 0;
        for (int row = 0; row < map.height(); ++row)
        {
            for (int column = 0; column < map.width(); ++column)
            {
                if (simple_platformer::contains(table.footprint({column, row}, profile), held))
                {
                    ++count;
                }
            }
        }
        return count;
    }
}

TEST_CASE("Preparing a profile builds every cell once", "[navigation][table]")
{
    const TileMap map = tests::TileMapBuilder({"........", "....##..", "########"});
    PlatformerConnectionTable table;
    REQUIRE_FALSE(table.isBuilt(Walker));

    table.prepare(map, Walker);
    REQUIRE(table.isBuilt(Walker));
    REQUIRE_FALSE(table.isBuilt(Climber));
    requireMatchesTheMap(table, map, Walker);

    const auto* stored = table.connections({0, 1}, Walker).data();
    REQUIRE_FALSE(table.connections({0, 1}, Walker).empty());
    table.prepare(map, Walker);
    REQUIRE(table.connections({0, 1}, Walker).data() == stored);
}

TEST_CASE("A break rebuilds only the cells whose footprint holds it", "[navigation][table]")
{
    TileMap map = tests::TileMapBuilder({"..........", "c.........", "c...g.....", "##########"})
                      .where('c', tests::Tile{}.blocksMovement().climbable())
                      .where('g', tests::Tile{}.blocksMovement().breaksInto('.'));
    PlatformerConnectionTable table;
    table.prepare(map, Walker);
    table.prepare(map, Climber);
    const int touched = cellsWhoseFootprintHolds(table, map, Walker, {4, 2}) +
                        cellsWhoseFootprintHolds(table, map, Climber, {4, 2});
    REQUIRE(touched > 0);
    REQUIRE(touched < 2 * 40);

    // Connections outside the break's footprint keep their storage.
    struct UnaffectedCell
    {
        Cell cell;
        PlatformerTraversalProfile profile;
        const simple_platformer::RouteConnection* stored;
    };

    std::vector<UnaffectedCell> unaffected;
    for (const auto& profile : {Walker, Climber})
    {
        for (int row = 0; row < map.height(); ++row)
        {
            for (int column = 0; column < map.width(); ++column)
            {
                const Cell cell{column, row};
                const auto& connections = table.connections(cell, profile);
                if (!connections.empty() &&
                    !simple_platformer::contains(table.footprint(cell, profile), {4, 2}))
                {
                    unaffected.push_back({cell, profile, connections.data()});
                }
            }
        }
    }
    REQUIRE_FALSE(unaffected.empty());
    REQUIRE(map.breakTile({4, 2}));
    table.applyRecordedTileBreaks(map);
    requireMatchesTheMap(table, map, Walker);
    requireMatchesTheMap(table, map, Climber);

    for (const auto& entry : unaffected)
    {
        REQUIRE(table.connections(entry.cell, entry.profile).data() == entry.stored);
    }

    // Applying the same break again leaves the rebuilt connections in place.
    const auto* rebuilt = table.connections({3, 2}, Walker).data();
    REQUIRE_FALSE(table.connections({3, 2}, Walker).empty());
    table.applyRecordedTileBreaks(map);
    REQUIRE(table.connections({3, 2}, Walker).data() == rebuilt);
}

TEST_CASE("A profile prepared after a break is built on the broken map", "[navigation][table]")
{
    TileMap map = tests::TileMapBuilder({"......", "......", "##g###"})
                      .where('g', tests::Tile{}.blocksMovement().breaksInto('.'));
    PlatformerConnectionTable table;
    table.prepare(map, Walker);
    REQUIRE(map.breakTile({2, 2}));

    table.prepare(map, Climber);
    requireMatchesTheMap(table, map, Walker);
    requireMatchesTheMap(table, map, Climber);
}

TEST_CASE("A connection table refuses what it does not hold", "[navigation][table]")
{
    const TileMap map = tests::TileMapBuilder({"....", "####"});
    PlatformerConnectionTable table;
    REQUIRE_THROWS_AS(table.connections({0, 0}, Walker), std::logic_error);
    REQUIRE_THROWS_AS(
        table.prepare(map, {{0.0F, 12.0F}, {}, tests::FixedStepSeconds}), std::invalid_argument);

    table.prepare(map, Walker);
    REQUIRE_THROWS_AS(table.connections({4, 0}, Walker), std::out_of_range);
    REQUIRE_THROWS_AS(table.footprint({0, -1}, Walker), std::out_of_range);

    const TileMap other = tests::TileMapBuilder({".....", "#####"});
    REQUIRE_THROWS_AS(table.prepare(other, Climber), std::logic_error);
}

TEST_CASE("Preparing navigation builds the profiles of platformer NPCs", "[navigation][table]")
{
    const TileMap map = tests::TileMapBuilder({"......", "......", "######"});
    simple_platformer::World world;
    // A player-like actor without a path follower is not navigated for.
    world.addActor(tests::ActorBuilder::sized({12.0F, 20.0F}).inCell({0, 1}).platforming());
    world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                       .inCell({2, 1})
                       .platforming()
                       .thinking({64.0F, 1.0F}));
    world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                       .inCell({4, 1})
                       .platforming()
                       .thinking({64.0F, 1.0F}));

    simple_platformer::prepareNavigation(map, world, tests::FixedStepSeconds);
    const PlatformerConnectionTable& table = world.platformerConnections();
    REQUIRE(table.isBuilt(Walker));
    REQUIRE_FALSE(table.isBuilt({{12.0F, 20.0F}, {}, tests::FixedStepSeconds}));
    requireMatchesTheMap(table, map, Walker);
}
