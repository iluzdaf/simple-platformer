#include <catch2/catch_test_macros.hpp>

#include <optional>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/world/sight.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/require_near.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"

namespace
{
    simple_platformer::Aabb boxIn(simple_platformer::GridPosition cell)
    {
        return simple_platformer::boxInCell(tests::TileSize, cell, {12.0F, 12.0F});
    }

    // Fully visible up to half in cover, hidden from three quarters.
    constexpr simple_platformer::CoverFade Fade{0.5F, 0.75F};
}

TEST_CASE("The fraction in cover is the share of a body over cover", "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"....", ".cc.", "...."}).where('c', tests::Tile().blocksSight());

    REQUIRE_NEAR(simple_platformer::fractionInCover(map, {{0.0F, 16.0F}, {16.0F, 16.0F}}), 0.0F);
    REQUIRE_NEAR(simple_platformer::fractionInCover(map, {{8.0F, 16.0F}, {16.0F, 16.0F}}), 0.5F);
    REQUIRE_NEAR(simple_platformer::fractionInCover(map, {{12.0F, 16.0F}, {16.0F, 16.0F}}), 0.75F);
    REQUIRE_NEAR(simple_platformer::fractionInCover(map, {{20.0F, 16.0F}, {16.0F, 16.0F}}), 1.0F);
    // Only the top half overlaps the cover row.
    REQUIRE_NEAR(simple_platformer::fractionInCover(map, {{12.0F, 24.0F}, {16.0F, 16.0F}}), 0.375F);
}

TEST_CASE("A body up to the conceal threshold stays fully visible", "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"....", ".cc.", "...."}).where('c', tests::Tile().blocksSight());
    const glm::vec2 viewer = simple_platformer::centerOf(boxIn({3, 2}));

    REQUIRE(
        simple_platformer::visibility(map, viewer, {{8.0F, 16.0F}, {16.0F, 16.0F}}, Fade) == 1.0F);
}

TEST_CASE(
    "Between the thresholds a body fades, and from the hide threshold it is hidden",
    "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"....", ".cc.", "...."}).where('c', tests::Tile().blocksSight());
    const glm::vec2 viewer = simple_platformer::centerOf(boxIn({3, 2}));

    // Five eighths in cover is halfway between the thresholds.
    REQUIRE_NEAR(
        simple_platformer::visibility(map, viewer, {{10.0F, 16.0F}, {16.0F, 16.0F}}, Fade), 0.5F);
    REQUIRE(
        simple_platformer::visibility(map, viewer, {{12.0F, 16.0F}, {16.0F, 16.0F}}, Fade) == 0.0F);
    REQUIRE(
        simple_platformer::visibility(map, viewer, {{20.0F, 16.0F}, {16.0F, 16.0F}}, Fade) == 0.0F);
}

TEST_CASE("A viewer in cover sees its own patch fully but not another", "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "...ccc.c", "........"})
            .where('c', tests::Tile().blocksSight());
    const glm::vec2 viewer = simple_platformer::centerOf(boxIn({3, 1}));

    REQUIRE(simple_platformer::visibility(map, viewer, boxIn({5, 1}), Fade) == 1.0F);
    REQUIRE(simple_platformer::visibility(map, viewer, boxIn({7, 1}), Fade) == 0.0F);
}

TEST_CASE("Cover hides nothing standing outside it", "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "...cc...", "........"})
            .where('c', tests::Tile().blocksSight());
    const glm::vec2 viewer = simple_platformer::centerOf(boxIn({0, 1}));

    // The cover between them does not matter: only standing in cover hides.
    REQUIRE(simple_platformer::visibility(map, viewer, boxIn({7, 1}), Fade) == 1.0F);
}

TEST_CASE("Without a viewer, cover alone decides visibility", "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"....", ".cc.", "...."}).where('c', tests::Tile().blocksSight());

    REQUIRE(simple_platformer::visibility(map, std::nullopt, boxIn({1, 1}), Fade) == 0.0F);
    REQUIRE_NEAR(
        simple_platformer::visibility(map, std::nullopt, {{10.0F, 16.0F}, {16.0F, 16.0F}}, Fade),
        0.5F);
    REQUIRE(simple_platformer::visibility(map, std::nullopt, boxIn({3, 1}), Fade) == 1.0F);
}

TEST_CASE("A wall breaks line of sight but hides nothing in the open", "[world][sight]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({".....", "..w..", "....."})
            .where('w', tests::Tile().blocksMovement().blocksSight());
    const glm::vec2 viewer = simple_platformer::centerOf(boxIn({0, 1}));
    const simple_platformer::Aabb target = boxIn({4, 1});

    REQUIRE_FALSE(simple_platformer::lineOfSight(map, viewer, simple_platformer::centerOf(target)));
    REQUIRE(simple_platformer::visibility(map, viewer, target, Fade) == 1.0F);
}
